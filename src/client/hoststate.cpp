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
#include "hoststate.h"
#include "sessionstate.h"
#include "brush.h"
#include "point.h"

#include "../shared/protocol.h"
#include "../shared/protocol.types.h"
#include "../shared/protocol.tools.h"
#include "../shared/protocol.flags.h"
#include "../shared/SHA1.h"
#include "../shared/templates.h" // for bswap

namespace network {

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
	msg->flags = protocol::client::None;
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
 * @param userlimit max. number of users
 * @param allowdraw allow drawing by default
 * @param allowchat allow chat by default
 * @pre user is logged in
 */
void HostState::host(const QString& title,
		const QString& password, quint16 width, quint16 height, int userlimit,
		bool allowdraw, bool allowchat)
{
	protocol::Instruction *msg = new protocol::Instruction;
	msg->command = protocol::admin::command::Create;
	msg->session_id = protocol::Global;
	msg->aux_data = userlimit;
	msg->aux_data2 = protocol::user_mode::None;
	if(allowdraw==false)
		fSet(msg->aux_data2, protocol::user_mode::Locked);
	if(allowchat==false)
		fSet(msg->aux_data2, protocol::user_mode::Mute);

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
		if(i->owner == localuser_.id()) {
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
		localuser_ = User(username_, msg->user_id, false, 0);
		emit loggedin();
	}
}

/**
 * If the session does not exist, it is added to the list. Otherwise
 * its description is updated.
 * @param msg SessionInfo message
 */
void HostState::handleSessionInfo(const protocol::SessionInfo *msg)
{
	qDebug() << "session info " << int(msg->session_id) << msg->title;
	bool updated = false;
	for(int i=0;i<sessions_.size();++i) {
		if(sessions_.at(i).id == msg->session_id) {
			sessions_.replace(i, Session(msg));
			if(mysessions_.contains(sessions_.at(i).id))
				mysessions_[sessions_.at(i).id]->update(sessions_.at(i));
			updated = true;
			break;
		}
	}
	if(updated==false)
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
		} else if(lastinstruction_ == protocol::admin::command::Alter) {
			qDebug() << "Warning: Ack for Instruction Alter not expected";
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

}

