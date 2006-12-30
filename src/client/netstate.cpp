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

#include "network.h"
#include "netstate.h"
#include "../shared/protocol.h"

namespace network {

HostState::HostState(QObject *parent)
	: QObject(parent), net_(0)
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
			case type::Error:
				handleError(static_cast<Error*>(msg));
				break;
			case type::Authentication:
				handleAuthentication(static_cast<Authentication*>(msg));
				break;
								 
			default:
				qDebug() << "unhandled message type " << int(msg->type);
				while(msg) {
					protocol::Message *next = msg->next;
					delete msg;
					msg = next;
				}
				break;
		}
	}
}

/**
 * Prepare the state machine for hosting a session.
 *
 * @param username username
 * @param title session title
 * @param password session password. If empty, no password is required to join.
 */
void HostState::host(const QString& username, const QString& title,
		const QString& password)
{
	state_ = LOGIN;
	username_ = username;
	title_ = title;
	password_ = password;
}

/**
 * Send Identifier message to log in. Server will disconnect or respond with
 * Authentication or HostInfo
 * @pre host or join should be called first to set the proper login sequence
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
		} else {
			// Join an existing session, but first we need the list of session
		}
	} else {
		// In other states, user info messages provide information about
		// other users.
	}
}

/**
 * @param msg Authentication message
 */
void HostState::handleAuthentication(protocol::Authentication *msg)
{
}

/**
 * @param msg Error message
 */
void HostState::handleError(protocol::Error *msg)
{
	// TODO
	emit error(tr("An error occured (%1)").arg(int(msg->code)));
}

}

