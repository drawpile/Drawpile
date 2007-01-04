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
#include "../shared/protocol.admin.h"
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

User::User(const QString& n, int i)
	: name(n), id(i)
{
}

HostState::HostState(QObject *parent)
	: QObject(parent), net_(0), newsession_(0), loggedin_(false)
{
}

/**
 * Receive messages and call the appropriate handlers.
 * @pre \a setConnection must have been called
 */
void HostState::receiveMessage()
{
	protocol::Message *msg;
	while((msg = net_->receive())) {
		switch(msg->type) {
			using namespace protocol;
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
			case type::ToolInfo:
				Q_ASSERT(selsession_);
				selsession_->handleToolInfo(static_cast<ToolInfo*>(msg));
				break;
			case type::StrokeInfo:
				Q_ASSERT(selsession_);
				selsession_->handleStrokeInfo(static_cast<StrokeInfo*>(msg));
				break;
			case type::StrokeEnd:
				Q_ASSERT(selsession_);
				selsession_->handleStrokeEnd(static_cast<StrokeEnd*>(msg));
				break;
			default:
				qDebug() << "unhandled message type " << int(msg->type);
				break;
		}
	}
	// Free the message(s)
	while(msg) {
		protocol::Message *next = msg->next;
		delete msg;
		msg = next;
	}
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
		selsession_ = 0;
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
	net_->send(msg);
}

/**
 * Create a new session with Instruction command. (Must be admin to do this.)
 * Server responds with an Error or Acknowledgement message.
 *
 * @param username username
 * @param title session title
 * @param password session password. If empty, no password is required to join.
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

	net_->send(msg);
}

/**
 * If there is only one session, automatically join it. Otherwise...
 */
void HostState::join()
{
	disconnect(this, SIGNAL(sessionsListed()), this, 0);
	connect(this, SIGNAL(sessionsListed()), this, SLOT(autoJoin()));
	listSessions();
}

/**
 * A password must be sent in response to an Authentication message.
 * The \a needPassword signal is used to notify when a password is required.
 * @param password password to send
 * @pre needPassword signal has been emitted
 */
void HostState::sendPassword(const QString& password)
{
	protocol::Password *msg = new protocol::Password;
	msg->session_id = passwordsession_;
	QByteArray pass = password.toUtf8();
	CSHA1 hash;
	hash.Update(reinterpret_cast<const uint8_t*>(pass.constData()),
			pass.length());
	hash.Update(reinterpret_cast<const uint8_t*>(passwordseed_.constData()),
			passwordseed_.length());
	hash.Final();
	hash.GetHash(reinterpret_cast<uint8_t*>(msg->data));
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
		if(i->owner == userid_) {
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
	} else if(sessions_.count()>1) {
		emit selectSession(sessions_);
	} else {
		join(sessions_.first().id);
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
		userid_ = msg->user_id;
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
 * Select active session
 * @param msg SessionSelect message
 */
void HostState::handleSessionSelect(const protocol::SessionSelect *msg)
{
	if(mysessions_.contains(msg->session_id)) {
		selsession_ = mysessions_.value(msg->session_id);
	} else {
		qDebug() << "Selected unsubscribed session" << int(msg->session_id);
	}
}

/**
 * @param msg Authentication message
 */
void HostState::handleAuthentication(const protocol::Authentication *msg)
{
	passwordseed_ = QByteArray(msg->seed, protocol::password_seed_size);
	passwordsession_ = msg->session_id;
	emit needPassword();
}

/**
 * Acknowledgement messages confirm previously sent commands.
 * @param msg Acknowledgement message
 */
void HostState::handleAck(const protocol::Acknowledgement *msg)
{
	qDebug() << "ack " << int(msg->event);
	if(msg->event == protocol::type::Instruction) {
		// Confirm instruction. Currently, the only instruction used is
		// Create, so this means the session was created. Automatically
		// join it (should be the latest session in list)
		disconnect(this, SIGNAL(sessionsListed()), this, 0);
		connect(this, SIGNAL(sessionsListed()), this, SLOT(joinLatest()));
		listSessions();
	} else if(msg->event == protocol::type::ListSessions) {
		// A full session list has been downloaded
		emit sessionsListed();
	} else if(msg->event == protocol::type::Subscribe) {
		// A session has been joined
		mysessions_.insert(newsession_->info().id, newsession_);
		emit joined(newsession_->info().id);
		newsession_ = 0;
	} else {
		qDebug() << "\tunhandled ack";
	}
}

/**
 * @param msg Error message
 */
void HostState::handleError(const protocol::Error *msg)
{
	qDebug() << "error " << int(msg->code);
	// TODO
	emit error(tr("An error occured (%1)").arg(int(msg->code)));
}

/**
 * @param parent parent HostState
 */
SessionState::SessionState(HostState *parent, const Session& info)
	: QObject(parent), host_(parent), info_(info)
{
	Q_ASSERT(parent);
}

/**
 * Get an image from received raster data. If received raster was empty,
 * a null image is loaded.
 * @param[out] loaded image is put here
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
 * Release raster data
 */
void SessionState::releaseRaster()
{
	raster_ = QByteArray();
}

/**
 * @param brush brush info to send
 */
void SessionState::sendToolInfo(const drawingboard::Brush& brush)
{
	const QColor hi = brush.color(1);
	const QColor lo = brush.color(0);
	const uchar hio = qRound(brush.opacity(1) * 100);
	const uchar loo = qRound(brush.opacity(0) * 100);
	protocol::ToolInfo *msg = new protocol::ToolInfo;

	msg->session_id = info_.id;;
	msg->tool_id = protocol::tool_type::Brush;
	msg->mode = protocol::tool_mode::Normal;
	msg->lo_color = lo.red() << 24 | lo.green() << 16 | lo.blue() << 8 | loo;
	msg->hi_color = hi.red() << 24 | hi.green() << 16 | hi.blue() << 8 | hio;
	msg->lo_size = brush.radius(0);
	msg->hi_size = brush.radius(1);
	msg->lo_hardness = qRound(brush.hardness(0)*100);
	msg->hi_hardness = qRound(brush.hardness(1)*100);
	host_->net_->send(msg);
}

/**
 * @param point stroke coordinates to send
 */
void SessionState::sendStrokeInfo(const drawingboard::Point& point)
{
	protocol::StrokeInfo *msg = new protocol::StrokeInfo;
	msg->session_id = info_.id;;
	msg->x = point.x();
	msg->y = point.y();
	msg->pressure = qRound(point.pressure()*255);
	host_->net_->send(msg);
}

void SessionState::sendStrokeEnd()
{
	protocol::StrokeEnd *msg = new protocol::StrokeEnd;
	msg->session_id = info_.id;;
	host_->net_->send(msg);
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
				users_.erase(i);
				emit userLeft(msg->user_id);
				break;
			}
		}
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
	} else {
		// Note. We make an assumption here that the data is sent in a 
		// sequential manner with no gaps in between.
		raster_.append(QByteArray(msg->data,msg->length));
		emit rasterReceived(100*(msg->offset+msg->length)/msg->size);
	}
}

/**
 * @param msg ToolInfo message
 */
void SessionState::handleToolInfo(const protocol::ToolInfo *msg)
{
	uchar r1 = (msg->hi_color & 0xff000000) >> 24;
	uchar g1 = (msg->hi_color & 0x00ff0000) >> 16;
	uchar b1 = (msg->hi_color & 0x0000ff00) >> 8;
	uchar a1 = (msg->hi_color & 0x000000ff);
	uchar r2 = (msg->lo_color & 0xff000000) >> 24;
	uchar g2 = (msg->lo_color & 0x00ff0000) >> 16;
	uchar b2 = (msg->lo_color & 0x0000ff00) >> 8;
	uchar a2 = (msg->lo_color & 0x000000ff);
	drawingboard::Brush brush(
			msg->hi_size,
			msg->hi_hardness/100.0,
			a1/100.0,
			QColor(r1,g1,b1));
	brush.setRadius2(msg->lo_size);
	brush.setColor2(QColor(r2,g2,b2));
	brush.setHardness2(msg->lo_hardness/100.0);
	brush.setOpacity(a2/100.0);
	emit toolReceived(msg->user_id, brush);
}

/**
 * @param msg StrokeInfo message
 */
void SessionState::handleStrokeInfo(const protocol::StrokeInfo *msg)
{
	emit strokeReceived(msg->user_id,
			drawingboard::Point(msg->x, msg->y, msg->pressure/255.0));
}

/**
 * @param msg StrokeEnd message
 */
void SessionState::handleStrokeEnd(const protocol::StrokeEnd *msg)
{
	emit strokeEndReceived(msg->user_id);
}

}

