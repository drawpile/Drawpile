/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#include <memory>

#include "../../config.h"

#include "network.h"
#include "netstate.h"
#include "../shared/protocol.h"
#include "../shared/protocol.admin.h"
#include "../shared/templates.h" // for bswap

namespace network {

HostState::HostState(QObject *parent)
	: QObject(parent), net_(0), newsession_(0)
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
 * Prepare the state machine for hosting a session.
 *
 * @param username username
 * @param title session title
 * @param password session password. If empty, no password is required to join.
 * @pre previous session creation (if any) has been finished
 */
void HostState::prepareHost(const QString& username, const QString& title,
		const QString& password, quint16 width, quint16 height)
{
	Q_ASSERT(newsession_ == 0);
	state_ = LOGIN;
	mode_ = HOSTSESSION;
	username_ = username;
	newsession_ = new SessionState(this);
	newsession_->init(title, password, width, height);
}

/**
 * Send Identifier message to log in. Server will disconnect or respond with
 * Authentication or HostInfo
 * @pre prepareHost or prepareJoin should be called first to set the proper login sequence
 * @pre \a setConnection must have been called.
 */
void HostState::login()
{
	Q_ASSERT(net_);
	protocol::Identifier *msg = new protocol::Identifier;
	memcpy(msg->identifier, protocol::identifier_string,
			protocol::identifier_size);
	msg->revision = protocol::revision;
	net_->send(msg);
}

/**
 * The password must be sent in response to an Authentication message.
 * The \a needPassword signal is used to notify when a password is required.
 * @param password password to send
 * @pre needPassword signal has been emitted
 */
void HostState::sendPassword(const QString& password)
{
	protocol::Password *msg = new protocol::Password;
	// TODO
	net_->send(msg);
}

/**
 * Send a ListSessions message to request a list of sessions.
 * The server will reply with SessionInfo messages, and an Acknowledgement
 * message to indicate a full list has been transmitted.
 *
 * When the complete list is downloaded, a sessionsList signal is emitted.
 */
void HostState::listSessions()
{
	// First clear out the old session list
	foreach(protocol::SessionInfo *i, sessions_)
		delete i;
	sessions_.clear();

	// Then request a new one
	protocol::ListSessions *msg = new protocol::ListSessions;
	net_->send(msg);
}

/**
 * The server will respond with Acknowledgement if join was succesfull.
 * @param id session id to join
 * @pre session list must include the id of the session
 */
void HostState::join(int id)
{
	bool found = false;
	// Get session parameters from list
	foreach(protocol::SessionInfo *i, sessions_) {
		if(i->identifier == id) {
			if(newsession_==0)
				newsession_ = new SessionState(this);
			newsession_->init(i);
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
	QList<protocol::SessionInfo*>::const_iterator i = sessions_.constEnd();
	do {
		--i;
		if((*i)->owner == userid_) {
			join((*i)->identifier);
			found = true;
			break;
		}
	} while(i!=sessions_.constBegin());
	Q_ASSERT(found==false);
}

/**
 * Host info provides information about the server and it's capabilities.
 * It is received in response to Identifier message to indicate that
 * the connection was accepted.
 *
 * User info will be sent in reply
 * @param msg HostInfo message
 */
void HostState::handleHostInfo(protocol::HostInfo *msg)
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
void HostState::handleUserInfo(protocol::UserInfo *msg)
{
	qDebug() << "user info";
	if(state_ == LOGIN) {
		// User info message while in LOGIN state finishes login sequence.
		userid_ = msg->user_id;
		state_ = JOIN;
		if(mode_ == HOSTSESSION) {
			// Now host a session
			newsession_->create();
		} else {
			// Join an existing session, but first we need the list of session
		}
	} else {
		// In other states, user info messages provide information about
		// other users.
	}
}

/**
 * Session info is appended to a list.
 * @param msg SessionInfo message
 */
void HostState::handleSessionInfo(protocol::SessionInfo *msg)
{
	qDebug() << "session info " << int(msg->identifier);
	sessions_.append(msg);
}

/**
 * @param msg Authentication message
 */
void HostState::handleAuthentication(protocol::Authentication *msg)
{
	qDebug() << "authentication";
}

/**
 * @param msg Acknowledgement message
 */
void HostState::handleAck(protocol::Acknowledgement *msg)
{
	qDebug() << "ack " << int(msg->event);
	if(msg->event == protocol::type::Instruction) {
		// Confirm instruction. Currently, the only instruction used is
		// Create, so this means the session was created. Proceed to request
		// a list of sessions
		disconnect(this, SIGNAL(sessionsListed()), this, 0);
		connect(this, SIGNAL(sessionsListed()), this, SLOT(joinLatest()));
		listSessions();
	} else if(msg->event == protocol::type::ListSessions) {
		// A full session list has been downloaded
		emit sessionsListed();
	} else if(msg->event == protocol::type::Subscribe) {
		// A session has been joined
		mysessions_.insert(newsession_->id(), newsession_);
		newsession_ = 0;
	} else {
		qDebug() << "\tunhandled ack";
	}
}

/**
 * @param msg Error message
 */
void HostState::handleError(protocol::Error *msg)
{
	qDebug() << "error " << int(msg->code);
	// TODO
	emit error(tr("An error occured (%1)").arg(int(msg->code)));
}

/**
 * @param parent parent HostState
 */
SessionState::SessionState(HostState *parent)
	: QObject(parent), host_(parent)
{
	Q_ASSERT(parent);
}

/**
 * Session state is initialized so create() can be called. For actual use,
 * init(SessionInfo*) has to be called.
 *
 * @param title session title
 * @param password session password. If empty, no password is set
 * @param width board width
 * @param height board height
 */
void SessionState::init(const QString& title, const QString& password,
		quint16 width, quint16 height)
{
	title_ = title;
	password_ = password;
	width_ = width;
	height_ = height;
}

/**
 * Session state is initialized based on actual information received
 * from the server. The session will then be ready for use.
 *
 * @param info SessionInfo message from which parameters are extracted
 */
void SessionState::init(const protocol::SessionInfo *info)
{
	Q_ASSERT(info);
	id_ = info->identifier;
	width_ = info->width;
	height_ = info->height;
	title_ = QString::fromUtf8(info->title);
}

/**
 * Use an Instruction message to create a new session. The server will
 * respond with an Ack or Error message.
 *
 * @pre init must have been called to set the session parameters.
 */
void SessionState::create()
{
	protocol::Instruction *msg = new protocol::Instruction;
	msg->command = protocol::admin::command::Create;
	msg->session = protocol::Global;
	msg->aux_data = 20; // User limit (TODO)
	msg->aux_data2 = protocol::user::None; // Default user mode (TODO)

	QByteArray title = title_.toUtf8();
	char *data = new char[sizeof(width_)+sizeof(height_)+title.length()];
	unsigned int off = 0;
	quint16 width = width_;
	quint16 height = height_;
	bswap(width);
	bswap(height);
	memcpy(data+off, &width, sizeof(width)); off += sizeof(width);
	memcpy(data+off, &height, sizeof(height)); off += sizeof(height);
	memcpy(data+off, title.constData(), title.length()); off += title.length();

	msg->length = off;
	msg->data = data;

	host_->net_->send(msg);
}

}

