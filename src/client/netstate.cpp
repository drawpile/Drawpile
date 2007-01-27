/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QDebug>
#include <QImage>
#include <memory>

#include "../../config.h"

#include "network.h"
#include "netstate.h"
#include "brush.h"
#include "point.h"

#include "../shared/protocol.h"
#include "../shared/protocol.types.h"
#include "../shared/protocol.tools.h"
#include "../shared/SHA1.h"
#include "../shared/templates.h" // for bswap

namespace network {

Session::Session(const protocol::SessionInfo *info)
	: id(info->session_id),
	owner(info->owner),
	title(QString::fromUtf8(info->title)),
	width(info->width),
	height(info->height)
{
}

User::User()
	: name(QString()), id(-1), locked(false)
{
}

User::User(const QString& n, int i)
	: name(n), id(i), locked(false)
{
}

HostState::HostState(QObject *parent)
	: QObject(parent), net_(0), newsession_(0), lastinstruction_(-1), loggedin_(false)
{
}

/**
 * Receive messages and call the appropriate handlers.
 * @pre \a setConnection must have been called
 */
void HostState::receiveMessage()
{
	protocol::Message *msg;
	// Get all available messages.
	// The message may be a "bundled message", that is it came from the network
	// with a single shared header and is now received as a linked list.
	while((msg = net_->receive())) { while(msg) {
		protocol::Message *next = msg->next;
		if(next!=0) {
			// The Message destructor will delete the entire list if we let it
			msg->next = 0;
			next->prev = 0;
		}
		switch(msg->type) {
			using namespace protocol;
			case type::StrokeInfo:
				Q_ASSERT(usersessions_.contains(msg->user_id));
				if(mysessions_.contains(usersessions_.value(msg->user_id)))
					if(mysessions_.value(usersessions_.value(msg->user_id))
						->handleStrokeInfo(static_cast<StrokeInfo*>(msg)))
						msg = 0;
				break;
			case type::StrokeEnd:
				Q_ASSERT(usersessions_.contains(msg->user_id));
				if(mysessions_.contains(usersessions_.value(msg->user_id)))
					if(mysessions_.value(usersessions_.value(msg->user_id))
						->handleStrokeEnd(static_cast<StrokeEnd*>(msg)))
						msg = 0;
				break;
			case type::ToolInfo:
				Q_ASSERT(usersessions_.contains(msg->user_id));
				if(mysessions_.contains(usersessions_.value(msg->user_id)))
					if(mysessions_.value(usersessions_.value(msg->user_id))
						->handleToolInfo(static_cast<ToolInfo*>(msg)))
						msg = 0;
				break;
			case type::Chat:
				Q_ASSERT(usersessions_.contains(msg->user_id));
				if(mysessions_.contains(usersessions_.value(msg->user_id)))
					mysessions_.value(usersessions_.value(msg->user_id))
						->handleChat(static_cast<Chat*>(msg));
				break;
			case type::UserInfo:
				handleUserInfo(static_cast<UserInfo*>(msg));
				break;
			case type::HostInfo:
				handleHostInfo(static_cast<HostInfo*>(msg));
				break;
			case type::SessionInfo:
				handleSessionInfo(static_cast<SessionInfo*>(msg));
				break;
			case type::Acknowledgement:
				handleAck(static_cast<Acknowledgement*>(msg));
				break;
			case type::Error:
				handleError(static_cast<Error*>(msg));
				break;
			case type::Authentication:
				handleAuthentication(static_cast<Authentication*>(msg));
				break;
			case type::SessionSelect:
				handleSessionSelect(static_cast<SessionSelect*>(msg));
				break;
			case type::Raster:
				if(mysessions_.contains(msg->session_id))
					mysessions_.value(msg->session_id)->handleRaster(
							static_cast<Raster*>(msg));
				else
					qDebug() << "received raster data for unsubscribed session" << int(msg->session_id);
				break;
			case type::Synchronize:
				if(mysessions_.contains(msg->session_id))
					mysessions_.value(msg->session_id)->handleSynchronize(
							static_cast<Synchronize*>(msg));
				else
					qDebug() << "received synchronize or unsubscribed session " << int(msg->session_id);
				break;
			case type::SyncWait:
				if(mysessions_.contains(msg->session_id))
					mysessions_.value(msg->session_id)->handleSyncWait(
							static_cast<SyncWait*>(msg));
				else
					qDebug() << "received synchronize or unsubscribed session " << int(msg->session_id);
				break;
			case type::SessionEvent:
				if(mysessions_.contains(msg->session_id))
					mysessions_.value(msg->session_id)->handleSessionEvent(
							static_cast<SessionEvent*>(msg));
				else
					qDebug() << "received event for unsubscribed session " << int(msg->session_id);
				break;

			default:
				qDebug() << "unhandled message type " << int(msg->type);
				break;
		}
		delete msg;
		msg = next;
	} }
}

/**
 * @param net connection to use
 */
void HostState::setConnection(Connection *net)
{
	net_ = net;
	if(net==0) {
		if(newsession_)
			delete newsession_;
		newsession_ = 0;
		usersessions_.clear();
		loggedin_ = false;
		foreach(SessionState *s, mysessions_) {
			emit parted(s->info().id);
			delete s;
		}
		mysessions_.clear();
	}
}

/**
 * Send Identifier message to log in. Server will disconnect or respond with
 * Authentication or HostInfo
 * @pre \a setConnection must have been called.
 */
void HostState::login(const QString& username)
{
	Q_ASSERT(net_);
	username_ = username;
	protocol::Identifier *msg = new protocol::Identifier;
	memcpy(msg->identifier, protocol::identifier_string,
			protocol::identifier_size);
	msg->revision = protocol::revision;
	msg->extensions = protocol::extensions::Chat;
	net_->send(msg);
}

/**
 * Create a new session with Instruction command. (Must be admin to do this.)
 * Server responds with an Error or Acknowledgement message.
 *
 * @param title session title
 * @param password session password. If empty, no password is required to join.
 * @param width board width
 * @param height board height
 * @pre user is logged in
 */
void HostState::host(const QString& title,
		const QString& password, quint16 width, quint16 height)
{
	protocol::Instruction *msg = new protocol::Instruction;
	msg->command = protocol::admin::command::Create;
	msg->session_id = protocol::Global;
	msg->aux_data = 20; // User limit (TODO)
	msg->aux_data2 = protocol::user_mode::None; // Default user mode (TODO)

	QByteArray tbytes = title.toUtf8();
	char *data = new char[sizeof(width)+sizeof(height)+tbytes.length()];
	unsigned int off = 0;
	quint16 w = width;
	quint16 h = height;
	bswap(w);
	bswap(h);
	memcpy(data+off, &w, sizeof(w)); off += sizeof(w);
	memcpy(data+off, &h, sizeof(h)); off += sizeof(h);
	memcpy(data+off, tbytes.constData(), tbytes.length()); off += tbytes.length();

	msg->length = off;
	msg->data = data;

	lastinstruction_ = msg->command;
	setsessionpassword_ = password;
	net_->send(msg);
}

/**
 * If there is only one session, automatically join it. Otherwise ask
 * the user to pick one.
 * @param title if not empty, automatically join the session with a matching title.
 */
void HostState::join(const QString& title)
{
	autojointitle_ = title;
	disconnect(this, SIGNAL(sessionsListed()), this, 0);
	connect(this, SIGNAL(sessionsListed()), this, SLOT(autoJoin()));
	listSessions();
}

/**
 * A password must be sent in response to an Authentication message.
 * The \a needPassword signal is used to notify when a password is required.
 * 
 * sendPassword is secure, a hash of the password + previously received seed
 * value is sent.
 * @param password password to send
 * @pre needPassword signal has been emitted
 */
void HostState::sendPassword(const QString& password)
{
	protocol::Password *msg = new protocol::Password;
	msg->session_id = passwordsession_;
	QByteArray pass = password.toUtf8();
	SHA1 hash;
	hash.Update(reinterpret_cast<const uint8_t*>(pass.constData()),
			pass.length());
	hash.Update(reinterpret_cast<const uint8_t*>(passwordseed_.constData()),
			passwordseed_.length());
	hash.Final();
	hash.GetHash(reinterpret_cast<uint8_t*>(msg->data));
	net_->send(msg);
}

/**
 * Sends an authentication request. Server will respond with an error
 * (InvalidRequest) or Authentication message.
 *
 * @param password admin password
 */
void HostState::becomeAdmin(const QString& password)
{
	protocol::Instruction *msg = new protocol::Instruction;
	msg->command = protocol::admin::command::Authenticate;
	msg->session_id = protocol::Global;
	sendadminpassword_ = password;
	lastinstruction_ = msg->command;
	net_->send(msg);
}

/**
 * Uses the Instruction message to set the server password. User
 * must have admin rights to do this.
 *
 * setPassword is insecure, the password is sent as cleartext.
 *
 * Server will respond with Ack/Instruction
 * @param password
 */
void HostState::setPassword(const QString& password)
{
	setPassword(password, protocol::Global);
}

/**
 * Send a password setting instruction. To set the global (server) password,
 * admin rights are needed. To set a session password, user must be the
 * session owner.
 * @param password password to set
 * @param session session id (use protocol::Global to set server password)
 */
void HostState::setPassword(const QString& password, int session)
{
	qDebug() << "sending password for session" << session;
	protocol::Instruction *msg = new protocol::Instruction;
	msg->command = protocol::admin::command::Password;
	msg->session_id = session;
	QByteArray passwd = password.toUtf8();

	msg->length = passwd.length();
	msg->data = new char[passwd.length()];
	memcpy(msg->data,passwd.constData(),passwd.length());

	lastinstruction_ = msg->command;
	net_->send(msg);
}

/**
 * Send a ListSessions message to request a list of sessions.
 * The server will reply with SessionInfo messages, and an Acknowledgement
 * message to indicate a full list has been transmitted.
 *
 * When the complete list is downloaded, a sessionsList signal is emitted.
 * @pre user is logged in
 */
void HostState::listSessions()
{
	// First clear out the old session list
	sessions_.clear();

	// Then request a new one
	protocol::ListSessions *msg = new protocol::ListSessions;
	net_->send(msg);
}

/**
 * The server will respond with Acknowledgement if join was succesfull.
 * @param id session id to join
 * @pre user must be logged in
 * @pre session list must include the id of the session
 */
void HostState::join(int id)
{
	bool found = false;
	// Get session parameters from list
	foreach(const Session &i, sessions_) {
		if(i.id == id) {
			newsession_ = new SessionState(this,i);
			found = true;
			break;
		}
	}
	Q_ASSERT(found);
	if(found==false)
		return;
	// Join the session
	qDebug() << "joining session " << id;
	protocol::Subscribe *msg = new protocol::Subscribe;
	msg->session_id = id;
	net_->send(msg);
}

/**
 * The session list is searched for the newest session that is owned
 * by the current user.
 * @pre session list has been refreshed
 */
void HostState::joinLatest()
{
	Q_ASSERT(sessions_.count() > 0);
	bool found = false;
	SessionList::const_iterator i = sessions_.constEnd();
	do {
		--i;
		if(i->owner == localuser_.id) {
			join(i->id);
			found = true;
			break;
		}
	} while(i!=sessions_.constBegin());
	Q_ASSERT(found);
}

/**
 * Automatically join a session if it is the only one in list
 * @pre session list has been refreshed
 */
void HostState::autoJoin()
{
	if(sessions_.count()==0) {
		emit noSessions();
	} else {
		if(autojointitle_.isEmpty()==false) {
			// Join a preselected session based on its title
			int id=-1;
			foreach(const Session &i, sessions_) {
				if(i.title.compare(autojointitle_)==0) {
					id = i.id;
					break;
				}
			}
			if(id==-1) {
				emit sessionNotFound();
				return;
			} else {
				join(id);
			}
		} else {
			// No preselected session
			if(sessions_.count()>1) {
				emit selectSession(sessions_);
			} else {
				join(sessions_.first().id);
			}
		}
	}
}

/**
 * Host info provides information about the server and its capabilities.
 * It is received in response to Identifier message to indicate that
 * the connection was accepted.
 *
 * User info will be sent in reply
 * @param msg HostInfo message
 */
void HostState::handleHostInfo(const protocol::HostInfo *msg)
{
	// Handle host info
	qDebug() << "host info";

	// Reply with user info
	protocol::UserInfo *user = new protocol::UserInfo;
	user->event = protocol::user_event::Login;
	QByteArray name = username_.toUtf8();
	user->length = name.length();
	user->name = new char[name.length()];
	memcpy(user->name,name.constData(), name.length());

	net_->send(user);
}

/**
 * User info message durin LOGIN state finishes the login sequence. In other
 * states it provides information about other users.
 * @param msg UserInfo message
 */
void HostState::handleUserInfo(const protocol::UserInfo *msg)
{
	qDebug() << "user info";
	if(loggedin_) {
		if(mysessions_.contains(msg->session_id)) {
			mysessions_.value(msg->session_id)->handleUserInfo(msg);
		} else {
			qDebug() << "received user info message for unsubscribed session " << msg->session_id;
		}
	} else {
		loggedin_ = true;
		localuser_.name = username_;
		localuser_.id = msg->user_id;
		emit loggedin();
	}
}

/**
 * Session info is appended to a list.
 * @param msg SessionInfo message
 */
void HostState::handleSessionInfo(const protocol::SessionInfo *msg)
{
	qDebug() << "session info " << int(msg->session_id) << msg->title;
	sessions_.append(msg);
}

/**
 * Select active session. Note that some users session might point
 * to one that is were are not subscribed to. In that case, drawing
 * commands for that user should be discarded.
 * @param msg SessionSelect message
 */
void HostState::handleSessionSelect(const protocol::SessionSelect *msg)
{
	usersessions_[msg->user_id] = msg->session_id;
	qDebug() << "user" << int(msg->user_id) << "active session" << int(msg->session_id);
}

/**
 * Authentication request. Authentication may be required when logging in,
 * joining a session or becoming administrator.
 * @param msg Authentication message
 */
void HostState::handleAuthentication(const protocol::Authentication *msg)
{
	passwordseed_ = QByteArray(msg->seed, protocol::password_seed_size);
	passwordsession_ = msg->session_id;
	if(lastinstruction_ == protocol::admin::command::Authenticate) {
		sendPassword(sendadminpassword_);
	} else {
		emit needPassword();
	}
}

/**
 * Acknowledgement messages confirm previously sent commands.
 * @param msg Acknowledgement message
 */
void HostState::handleAck(const protocol::Acknowledgement *msg)
{
	if(msg->session_id != protocol::Global) {
		// Handle session acks
		if(msg->event == protocol::type::Subscribe) {
			// Special case. When subscribing, session is not yet in the list
			mysessions_.insert(newsession_->info().id, newsession_);
			newsession_->select();
			// TODO, use newsession_-> when server supports session passwds
			if(setsessionpassword_.isEmpty()==false) {
				newsession_->setPassword(setsessionpassword_);
				setsessionpassword_ = "";
			}
			emit joined(newsession_->info().id);
			newsession_ = 0;
		} else {
			// Let the session handle the message
			if(mysessions_.contains(msg->session_id)) {
				mysessions_.value(msg->session_id)->handleAck(msg);
			} else {
				qDebug() << "received ack for unsubscribed session"
					<< int(msg->session_id);
			}
		}
		return;
	}
	// Handle global acks
	if(msg->event == protocol::type::Instruction) {
		if(lastinstruction_ == protocol::admin::command::Create) {
			// Automatically join the newest session created
			disconnect(this, SIGNAL(sessionsListed()), this, 0);
			connect(this, SIGNAL(sessionsListed()), this, SLOT(joinLatest()));
			listSessions();
			qDebug() << "session created, joining...";
		} else if(lastinstruction_ == protocol::admin::command::Password) {
			// Password accepted
			qDebug() << "password set";
		} else {
			qFatal("BUG: unhandled lastinstruction_");
		}
		lastinstruction_ = -1;
	} else if(msg->event == protocol::type::ListSessions) {
		// A full session list has been downloaded
		emit sessionsListed();
	} else if(msg->event == protocol::type::Password) {
		if(lastinstruction_ == protocol::admin::command::Authenticate) {
			emit becameAdmin();
		}
	} else {
		qDebug() << "unhandled host ack" << int(msg->event);
	}
}

/**
 * @param msg Error message
 */
void HostState::handleError(const protocol::Error *msg)
{
	QString errmsg;
	switch(msg->code) {
		using namespace protocol::error;
		case UserLimit: errmsg = tr("Server full."); break;
		case SessionLimit: errmsg = tr("No room for more sessions."); break;
		case NoSessions: errmsg = tr("No sessions."); break;
		case UnknownSession: errmsg = tr("No such session found."); break;
		case SessionFull: errmsg = tr("Session full."); break;
		case TooSmall: errmsg = tr("Board too small."); break;
		case SyncFailure: errmsg = tr("Board synchronization failed, try again."); break;
		case PasswordFailure: errmsg = tr("Incorrect password."); break;
		case SessionLost: errmsg = tr("Session lost."); break;
		case TooLong: errmsg = tr("Name too long."); break;
		case NotUnique: errmsg = tr("Name already in use."); break;
		case InvalidRequest: errmsg = tr("Invalid request"); break;
		case Unauthorized: errmsg = tr("Unauthorized action"); break;
		default: errmsg = tr("Error code %1").arg(int(msg->code));
	}
	qDebug() << "error" << errmsg << "for session" << msg->session_id;
	emit error(errmsg);
}

/**
 * @param parent parent HostState
 * @param info session information
 */
SessionState::SessionState(HostState *parent, const Session& info)
	: QObject(parent), host_(parent), info_(info), rasteroffset_(0),lock_(false),bufferdrawing_(true)
{
	Q_ASSERT(parent);
	users_.append(host_->localuser_);
}

/**
 * @param id user id
 */
const User *SessionState::user(int id) const
{
	foreach(const User& u, users_) {
		if(u.id == id)
			return &u;
	}
	return 0;
}

/**
 * Get an image from received raster data. If received raster was empty,
 * a null image is loaded.
 * @param[out] image loaded image is put here
 * @retval false if buffer contained invalid data
 */
bool SessionState::sessionImage(QImage &image) const
{
	QImage img;
	if(raster_.isEmpty()) {
		image = QImage();
	} else {
		if(img.loadFromData(raster_)==false)
			return false;
		image = img;
	}
	return true;
}

/**
 * @retval true if uploading
 */
bool SessionState::isUploading() const
{
	if(raster_.length()>0 && rasteroffset_>0)
		return true;
	return false;
}

/**
 * Release raster data
 */
void SessionState::releaseRaster()
{
	raster_ = QByteArray();
}

/**
 * Start sending raster data.
 * The data will be sent in pieces, interleaved with other messages.
 * @param raster raster data buffer
 */
void SessionState::sendRaster(const QByteArray& raster)
{
	raster_ = raster;
	rasteroffset_ = 0;
	sendRasterChunk();
}

void SessionState::sendRasterChunk()
{
	unsigned int chunklen = 1024*4;
	if(rasteroffset_ + chunklen > unsigned(raster_.length()))
		chunklen = raster_.length() - rasteroffset_;
	if(chunklen==0) {
		rasteroffset_ = 0;
		releaseRaster();
		return;
	}
	protocol::Raster *msg = new protocol::Raster;
	msg->session_id = info_.id;
	msg->offset = rasteroffset_;
	msg->length = chunklen;
	msg->size = raster_.length();
	msg->data = new char[chunklen];
	memcpy(msg->data, raster_.constData()+rasteroffset_, chunklen);
	rasteroffset_ += chunklen;
	host_->net_->send(msg);
	emit rasterSent(100*rasteroffset_/raster_.length());
}

/**
 * Send a SessionSelect message to indicate this session as the one
 * where drawing occurs.
 */
void SessionState::select()
{
	protocol::SessionSelect *msg = new protocol::SessionSelect;
	msg->session_id = info_.id;
	host_->usersessions_[host_->localuser_.id] = info_.id;
	host_->net_->send(msg);
}

/**
 * Set the session password.
 * @param password password to set
 */
void SessionState::setPassword(const QString& password)
{
	host_->setPassword(password, info_.id);
}

/**
 * @param id user id
 * @pre user is session owner
 */
void SessionState::kickUser(int id)
{
	protocol::SessionEvent *msg = new protocol::SessionEvent;
	msg->session_id = info_.id;
	msg->action = protocol::session_event::Kick;
	msg->target = id;
	host_->net_->send(msg);
}

/**
 * @param id user id
 * @param lock lock status
 * @pre user is session owner
 */
void SessionState::lockUser(int id, bool lock)
{
	protocol::SessionEvent *msg = new protocol::SessionEvent;
	msg->session_id = info_.id;
	if(lock)
		msg->action = protocol::session_event::Lock;
	else
		msg->action = protocol::session_event::Unlock;
	msg->target = id;
	host_->net_->send(msg);
}

/**
 * @param brush brush info to send
 */
void SessionState::sendToolInfo(const drawingboard::Brush& brush)
{
	const QColor hi = brush.color(1);
	const QColor lo = brush.color(0);
	protocol::ToolInfo *msg = new protocol::ToolInfo;

	msg->session_id = info_.id;;
	msg->tool_id = protocol::tool_type::Brush;
	msg->mode = protocol::tool_mode::Normal;
	msg->lo_color[0] = lo.red();
	msg->lo_color[1] = lo.green();
	msg->lo_color[2] = lo.blue();
	msg->lo_color[3] = qRound(brush.opacity(0) * 255);
	msg->hi_color[0] = hi.red();
	msg->hi_color[1] = hi.green();
	msg->hi_color[2] = hi.blue();
	msg->hi_color[3] = qRound(brush.opacity(1) * 255);
	msg->lo_size = brush.radius(0);
	msg->hi_size = brush.radius(1);
	msg->lo_hardness = qRound(brush.hardness(0)*255);
	msg->hi_hardness = qRound(brush.hardness(1)*255);
	host_->net_->send(msg);
}

/**
 * @param point stroke coordinates to send
 */
void SessionState::sendStrokeInfo(const drawingboard::Point& point)
{
	protocol::StrokeInfo *msg = new protocol::StrokeInfo;
	msg->session_id = info_.id;;
	msg->x = uint16_t(point.x()) << 2 | point.subx();
	msg->y = uint16_t(point.y()) << 2 | point.suby();
	msg->pressure = qRound(point.pressure()*255);
	host_->net_->send(msg);
}

void SessionState::sendStrokeEnd()
{
	protocol::StrokeEnd *msg = new protocol::StrokeEnd;
	msg->session_id = info_.id;;
	host_->net_->send(msg);
}

void SessionState::sendAckSync()
{
	protocol::Acknowledgement *msg = new protocol::Acknowledgement;
	msg->session_id = info_.id;
	msg->event = protocol::type::SyncWait;
	host_->net_->send(msg);
}

void SessionState::sendChat(const QString& message)
{
	QByteArray arr = message.toUtf8();
	protocol::Chat *msg = new protocol::Chat;
	msg->session_id = info_.id;
	msg->length = arr.length();
	msg->data = new char[arr.length()];
	memcpy(msg->data,arr.constData(),arr.length());
	host_->net_->send(msg);
}

/**
 * @param msg Acknowledgement message
 */
void SessionState::handleAck(const protocol::Acknowledgement *msg)
{
	if(msg->event == protocol::type::SyncWait) {
		emit syncDone();
	} else if(msg->event == protocol::type::SessionSelect) {
		// Ignore session select ack
	} else if(msg->event == protocol::type::Raster) {
		sendRasterChunk();
	} else {
		qDebug() << "unhandled session ack" << int(msg->event);
	}
}

/**
 * @param msg UserInfo message
 */
void SessionState::handleUserInfo(const protocol::UserInfo *msg)
{
	if(msg->event == protocol::user_event::Join) {
		users_.append(User(msg->name, msg->user_id));
		emit userJoined(msg->user_id);
	} else if(msg->event == protocol::user_event::Leave ||
			msg->event == protocol::user_event::Disconnect ||
			msg->event == protocol::user_event::BrokenPipe ||
			msg->event == protocol::user_event::TimedOut ||
			msg->event == protocol::user_event::Dropped ||
			msg->event == protocol::user_event::Kicked) {
		UserList::iterator i = users_.begin();
		for(;i!=users_.end();++i) {
			if(i->id == msg->user_id) {
				emit userLeft(msg->user_id);
				users_.erase(i);
				break;
			}
		}
		host_->usersessions_.remove(msg->user_id);
	} else {
		qDebug() << "unhandled user event " << int(msg->event);
	}
}

/**
 * Receive raster data. When joining an empty session, a raster message
 * with all fields zeroed is received.
 * The rasterReceived signal is emitted every time data is received.
 * @param msg Raster data message
 */
void SessionState::handleRaster(const protocol::Raster *msg)
{
	if(msg->size==0) {
		// Special case, zero size raster
		emit rasterReceived(100);
		flushDrawBuffer();
	} else {
		if(msg->offset==0) {
			// Raster data has just started or has been restarted.
			raster_.truncate(0);
		}
		// Note. We make an assumption here that the data is sent in a 
		// sequential manner with no gaps in between.
		raster_.append(QByteArray(msg->data,msg->length));
		if(msg->offset+msg->length<msg->size) {
			emit rasterReceived(99*(msg->offset+msg->length)/msg->size);
		} else {
			emit rasterReceived(100);
			flushDrawBuffer();
		}
	}
}

/**
 * A synchronize request causes the client to start transmitting a copy of
 * the drawingboard as soon as the user stops drawing.
 * @param msg Synchronize message
 */
void SessionState::handleSynchronize(const protocol::Synchronize *msg)
{
	emit syncRequest();
}

/**
 * Client will enter SyncWait state. The board will be locked as soon as
 * the current stroke is finished. The client will respond with Ack/SyncWait
 * when the board is locked. Ack/sync from the server will unlock the board.
 * @param msg SyncWait message
 */
void SessionState::handleSyncWait(const protocol::SyncWait *msg)
{
	emit syncWait();
}

/**
 * Received session events contain information about other users in the
 * session.
 * @param msg SessionEvent message
 */
void SessionState::handleSessionEvent(const protocol::SessionEvent *msg)
{
	User *user = 0;
	if(msg->target != protocol::null_user) {
		for(int i=0;i<users_.size();++i) {
			if(users_.at(i).id == msg->target) {
				user = &users_[i];
				break;
			}
		}
		if(user==0) {
			qDebug() << "received SessionEvent for user" << int(msg->target)
				<< "not part of the session";
			return;
		}
	}

	switch(msg->action) {
		using namespace protocol::session_event;
		case Lock:
			if(user) {
				user->locked = true;
				emit userLocked(msg->target, true);
			} else {
				lock_ = true;
				emit sessionLocked(true);
			}
			break;
		case Unlock:
			if(user) {
				user->locked = false;
				emit userLocked(msg->target, false);
			} else {
				lock_ = false;
				emit sessionLocked(false);
			}
			break;
		case Kick:
			emit userKicked(msg->target);
			break;
		case Delegate:
			info_.owner = msg->target;
			emit ownerChanged();
			break;
		default:
			qDebug() << "unhandled session event action" << int(msg->action);
	}
}

/**
 * @param msg ToolInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleToolInfo(protocol::ToolInfo *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	drawingboard::Brush brush(
			msg->hi_size,
			msg->hi_hardness/255.0,
			msg->hi_color[3]/255.0,
			QColor(msg->hi_color[0], msg->hi_color[1], msg->hi_color[2]));
	brush.setRadius2(msg->lo_size);
	brush.setColor2(QColor(msg->lo_color[0], msg->lo_color[1], msg->lo_color[2]));
	brush.setHardness2(msg->lo_hardness/255.0);
	brush.setOpacity2(msg->lo_color[3]/255.0);
	emit toolReceived(msg->user_id, brush);
	return false;
}

/**
 * @param msg StrokeInfo message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStrokeInfo(protocol::StrokeInfo *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	Q_ASSERT(msg->type == protocol::type::StrokeInfo);
	qreal x = (msg->x >> 2) + (msg->x&0x3) / 3.0;
	qreal y = (msg->y >> 2) + (msg->y&0x3) / 3.0;
	emit strokeReceived(
			msg->user_id,
			drawingboard::Point(
				x,
				y,
				msg->pressure/255.0
				)
			);
	return false;
}

/**
 * @param msg StrokeEnd message
 * @retval true message was buffered, don't delete
 */
bool SessionState::handleStrokeEnd(protocol::StrokeEnd *msg)
{
	if(bufferdrawing_) {
		drawbuffer_.enqueue(msg);
		return true;
	}
	emit strokeEndReceived(msg->user_id);
	return false;
}

/**
 * @param msg chat message
 */
void SessionState::handleChat(const protocol::Chat *msg)
{
	const User *u = user(msg->user_id);
	QString str = QString::fromUtf8(msg->data, msg->length);
	emit chatMessage(u?u->name:"<unknown>", str);
}

/**
 * Flush the drawing buffer and emit the commands.
 * @post drawing buffer is disabled for the rest of the session
 */
void SessionState::flushDrawBuffer()
{
	bufferdrawing_ = false;
	while(drawbuffer_.isEmpty() == false) {
		protocol::Message *msg = drawbuffer_.dequeue();
		if(msg->type == protocol::type::StrokeInfo)
			handleStrokeInfo(static_cast<protocol::StrokeInfo*>(msg));
		else if(msg->type == protocol::type::StrokeEnd)
			handleStrokeEnd(static_cast<protocol::StrokeEnd*>(msg));
		else
			handleToolInfo(static_cast<protocol::ToolInfo*>(msg));

		delete msg;
	}
}


}

