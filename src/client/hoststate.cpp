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

#include "../config.h"

#include "network.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "version.h"

#include "../shared/protocol.h"
#include "../shared/protocol.errors.h"
#include "../shared/protocol.tools.h"
#include "../shared/protocol.flags.h"
#include "../shared/SHA1.h"
#include "../shared/templates.h" // for bswap

namespace network {

HostState::HostState(QObject *parent)
	: QObject(parent), net_(0), session_(0), lastsessioninstr_(-1), loggedin_(false)
{
}

/**
 * Receive messages and call the appropriate handlers.
 * @pre \a setConnection must have been called
 */
void HostState::receiveMessage()
{
	using namespace protocol;
	Message *msg;
	// Get all available messages.
	// The message may be a "bundled message", that is it came from the network
	// with a single shared header and is now received as a linked list.
	// Host and session messages are not mixed in bundles.
	while((msg = net_->receive())) {

		if(msg->session_id != 0) {
			// Messages with a session number go to the session,
			// with a few exceptions.
			
			if(msg->type == Message::Acknowledgement
					&& (static_cast<Acknowledgement*>(msg)->event == protocol::Message::Subscribe ||
						static_cast<Acknowledgement*>(msg)->event == protocol::Message::Password)) {
				// Special case, session subscription ack.
				session_->select();
				if(setsessionpassword_.isEmpty()==false) {
					session_->setPassword(setsessionpassword_);
					setsessionpassword_ = "";
				}
				emit joined(session_->info().id);
			} else if(msg->type == Message::SessionInfo) {
				// Special case, session info
				handleSessionInfo(static_cast<SessionInfo*>(msg));
				delete msg;
			} else {
				// Regular session messages.
				if(session_ != 0) {
					session_->handleMessage(msg);
				} else {
					qDebug() << "got session message" << int(msg->type) << "for unsubscribed session" << int(msg->session_id);
					delete msg;
				}
			}
		} else if(msg->isSelected) {
			// Selected messages always go straight to the session
			if(session_ != 0) {
				session_->handleMessage(msg);
			} else {
					qDebug() << "got selected session message" << int(msg->type) << "even though no sessions are joined";
					delete msg;
			}
		} else do {
			// Handle host messages
			Message *next = msg->next;
			switch(msg->type) {
				case Message::UserInfo:
					handleUserInfo(static_cast<UserInfo*>(msg));
					break;
				case Message::HostInfo:
					handleHostInfo(static_cast<HostInfo*>(msg));
					break;
				case Message::Acknowledgement:
					handleAck(static_cast<Acknowledgement*>(msg));
					break;
				case Message::Error:
					handleError(static_cast<Error*>(msg));
					break;
				case Message::PasswordRequest:
					handleAuthentication(static_cast<PasswordRequest*>(msg));
					break;

				default:
					qDebug() << "unhandled host message type" << int(msg->type);
					break;
			}
			delete msg;
			msg = next;
		} while(msg);
	}
}

/**
 * @param net connection to use
 */
void HostState::setConnection(Connection *net)
{
	net_ = net;
	if(net==0) {
		loggedin_ = false;
		sessions_.clear();
		if(session_) {
			emit parted(session_->info().id);
			delete session_;
			session_ = 0;
		}
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
	protocol::Identifier *msg = new protocol::Identifier(
			protocol::revision,
			version::level,
			protocol::client::None,
			protocol::extensions::Chat
			);
	memcpy(msg->identifier, protocol::identifier_string,
			protocol::identifier_size);
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
	const QByteArray tbytes = title.toUtf8();
	
	protocol::SessionInstruction *msg = new protocol::SessionInstruction(
			protocol::SessionInstruction::Create,
			width,
			height,
			protocol::user::None,
			userlimit,
			0, // flags (unused)
			tbytes.length(),
			(tbytes.length() ? new char[tbytes.length()] : 0)
			);
	
	msg->session_id = protocol::Global;
	if(allowdraw==false)
		fSet(msg->user_mode, static_cast<quint8>(protocol::user::Locked));
	if(allowchat==false)
		fSet(msg->user_mode, static_cast<quint8>(protocol::user::Mute));
	
	if (msg->title_len != 0)
	{
		memcpy(msg->title, tbytes.constData(), tbytes.length());
	}
	
	lastsessioninstr_ = msg->action;
	setsessionpassword_ = password;
	net_->send(msg);
}

/**
 * If there is only one session, automatically join it. Otherwise ask
 * the user to pick one.
 *
 * A list of sessions is requested. When the list is received (sessionsListed signal is emitted), autoJoin is called.
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
	const QByteArray pass = password.toUtf8();
	SHA1 hash;
	hash.Update(reinterpret_cast<const quint8*>(pass.constData()),
			pass.length());
	hash.Update(reinterpret_cast<const quint8*>(passwordseed_.constData()),
			passwordseed_.length());
	hash.Final();
	hash.GetHash(reinterpret_cast<quint8*>(msg->data));
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
	protocol::Authenticate *msg = new protocol::Authenticate;
	sendadminpassword_ = password;
	
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
	const QByteArray passwd = password.toUtf8();
	
	protocol::SetPassword *msg = new protocol::SetPassword(
			passwd.length(),
			new char[passwd.length()]
			);
	
	msg->session_id = session;
	memcpy(msg->password,passwd.constData(),passwd.length());
	
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
 * @pre user must not be joined to any other session
 */
void HostState::join(int id)
{
	Q_ASSERT(session_ == 0);
	bool found = false;
	// Get session parameters from list
	foreach(const Session &i, sessions_) {
		if(i.id == id) {
			session_ = new SessionState(this,i);
			found = true;
			break;
		}
	}
	Q_ASSERT(found);
	if(found==false)
		return;
	// Join the session
	protocol::Subscribe *msg = new protocol::Subscribe;
	msg->session_id = id;
	net_->send(msg);
}

/**
 * The session list is searched for the newest session that is owned
 * by the current user.
 * @pre session list contains a session owned by this user
 */
void HostState::joinLatest()
{
	Q_ASSERT(sessions_.count() > 0);
	SessionList::const_iterator i = sessions_.constEnd();
	do {
		--i;
		if(i->owner == localuser_.id()) {
			join(i->id);
			return;
		}
	} while(i!=sessions_.constBegin());
	Q_ASSERT(false);
}

/**
 * Attempt to automatically join a session
 *
 * Emits noSessions if no session are available, sessionNotFound if no
 * session matches the optionally preselected title is found and
 * selectSession if there are more than one available session and user
 * intervention is required.
 *
 * @pre session list has been refreshed
 * @post a session is either joined or a signal is emitted indicating the error condition or need for user intervention.
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
	
	const QByteArray name = username_.toUtf8();
	
	// Reply with user info
	protocol::UserInfo *user = new protocol::UserInfo(
			0, // mode (ignored)
			protocol::UserInfo::Login,
			name.length(),
			new char[name.length()]
			);
	
	memcpy(user->name,name.constData(), name.length());
	
	net_->send(user);
}

/**
 * User info message durin LOGIN state finishes the login sequence. In other
 * states it provides information about other users, and is sent to the
 * appropriate session.
 * @param msg UserInfo message
 */
void HostState::handleUserInfo(const protocol::UserInfo *msg)
{
	Q_ASSERT(loggedin_ == false);
	loggedin_ = true;
	localuser_ = User(username_, msg->user_id, false, 0);
	emit loggedin();
}

/**
 * If the session does not exist, it is added to the list. Otherwise
 * its description is updated.
 * @param msg SessionInfo message
 */
void HostState::handleSessionInfo(const protocol::SessionInfo *msg)
{
	bool updated = false;
	for(int i=0;i<sessions_.size();++i) {
		if(sessions_.at(i).id == msg->session_id) {
			sessions_.replace(i, Session(msg));

			// If the updated session info was the joined session,
			// update the actual session too.
			if(session_->info().id == sessions_.at(i).id)
				session_->update(sessions_.at(i));
			updated = true;
			break;
		}
	}
	if(updated==false)
		sessions_.append(msg);
}

/**
 * Authentication request. Authentication may be required when logging in,
 * joining a session or becoming administrator.
 * @param msg Authentication message
 */
void HostState::handleAuthentication(const protocol::PasswordRequest *msg)
{
	passwordseed_ = QByteArray(msg->seed, protocol::password_seed_size);
	passwordsession_ = msg->session_id;
	if(msg->session_id == protocol::Global) {
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
	// Handle global acks
	switch (msg->event) {
		case protocol::Message::SessionInstruction:
			if(lastsessioninstr_ == protocol::SessionInstruction::Create) {
				// Automatically join the newest session created
				disconnect(this, SIGNAL(sessionsListed()), this, 0);
				connect(this, SIGNAL(sessionsListed()), this, SLOT(joinLatest()));
				listSessions();
			} else if(lastsessioninstr_ == protocol::SessionInstruction::Alter) {
				// FIXME: User limit changed?
				qDebug() << "Warning: Ack for Instruction Alter not expected";
			} else {
				qFatal("BUG: unhandled lastsessioninstr_");
			}
			lastsessioninstr_ = -1;
			break;
		case protocol::Message::SetPassword:
			// Password accepted
			break;
		case protocol::Message::ListSessions:
			// A full session list has been downloaded
			emit sessionsListed();
			break;
		case protocol::Message::Password:
			if (msg->session_id == protocol::Global) {
				emit becameAdmin();
			}
			break;
		default:
			qDebug() << "unhandled host ack" << int(msg->event);
			break;
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
		case InvalidSize: errmsg = tr("Board has invalid size."); break;
		case SyncFailure: errmsg = tr("Board synchronization failed, try again."); break;
		case PasswordFailure: errmsg = tr("Incorrect password."); break;
		case SessionLost: errmsg = tr("Session lost."); break;
		case TooLong: errmsg = tr("Name too long."); break;
		case NotUnique: errmsg = tr("Name already in use."); break;
		case InvalidRequest: errmsg = tr("Invalid request."); break;
		case Unauthorized: errmsg = tr("Unauthorized action."); break;
		case ImplementationMismatch: errmsg = tr("Client version mismatch."); break;
		default: errmsg = tr("Error code %1").arg(int(msg->code));
	}
	qDebug() << "error" << errmsg << "for session" << msg->session_id;
	emit error(errmsg);
}

}

