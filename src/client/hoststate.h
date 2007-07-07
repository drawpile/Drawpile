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
#ifndef HOSTSTATE_H
#define HOSTSTATE_H

#include <QObject>

#include "sessioninfo.h"

namespace protocol {
	class HostInfo;
	class UserInfo;
	class SessionInfo;
	class SessionSelect;
	class PasswordRequest;
	class Error;
	class Acknowledgement;
};

namespace network {

class Connection;
class SessionState;

//! Network state machine
/**
 * This class handles the state of a host connection.
 */
class HostState : public QObject {
	Q_OBJECT
	friend class SessionState;
	public:
		//! Construct a HostState object
		HostState(QObject *parent);

		//! Get local user info
		const User& localUser() const { return localuser_; }

		//! Get the session state
		SessionState *session() { return session_; }

		//! Set network connection object to use
		void setConnection(Connection *net);

		//! Get the used network connection object
		Connection *connection() { return net_; }

		//! Initiate login sequence
		void login(const QString& username);

		//! Send a password
		void sendPassword(const QString& password);

		//! Try to elevate to admin status
		void becomeAdmin(const QString& password);

		//! Host a session
		void host(const QString& title, const QString& password,
				quint16 width, quint16 height, int userlimit,
				bool allowdraw, bool allowchat);

		//! Try joining automatically
		void join(const QString& title = QString());

		//! Join a specific session
		void join(int id);

		//! Set server password
		void setPassword(const QString& password);

	public slots:
		//! Get a message from the network handler object
		void receiveMessage();

	signals:
		//! A password must be requested from the user
		void needPassword();

		//! Login sequence completed succesfully
		void loggedin();

		//! Admin password accepted
		void becameAdmin();

		//! Session joined succesfully
		/**
		 * @param id session id
		 */
		void joined(int id);

		//! Session left
		/**
		 * @param id session id
		 */
		void parted(int id);

		//! An error message was received from the host
		void error(const QString& message);

		//! Host has no sessions, cannot join
		void noSessions();

		//! The session we tried to join didn't exist
		void sessionNotFound();

		//! A session should be selected from the list and joined
		void selectSession(const network::SessionList& sessions);

		//! Session list was refreshed
		void sessionsListed();

	private slots:
		//! Join the latest session that the local user owns.
		void joinLatest();

		//! Automatically join the only session available
		void autoJoin();

	private:
		//! Set server or session password
		void setPassword(const QString& password, int session);

		//! Refresh the list of sessions
		void listSessions();

		//! Handle a HostInfo message
		void handleHostInfo(const protocol::HostInfo *msg);

		//! Handle a UserInfo message
		void handleUserInfo(const protocol::UserInfo *msg);

		//! Handle a SessionInfo message
		void handleSessionInfo(const protocol::SessionInfo *msg);

		//! Handle authentication request
		void handleAuthentication(const protocol::PasswordRequest *msg);

		//! Handle Acknowledgements
		void handleAck(const protocol::Acknowledgement *msg);

		//! Handle errors
		void handleError(const protocol::Error *msg);

		//! Connection to the server
		Connection *net_;

		//! User name to log in with
		QString username_;

		//! Seed for generating password hash (received from the server)
		QByteArray passwordseed_;

		//! Session ID of the session for which the password is sent
		int passwordsession_;

		//! Title of the session to autojoin
		/**
		 * If empty, the only available session is joined or if there are
		 * many sessions, the user is asked.
		 */
		QString autojointitle_;

		//! Info about the local user as received from server during login.
		User localuser_;

		//! The currently joined session
		SessionState *session_;

		//! List of sessions on the server
		SessionList sessions_;

		//! Last used session instruction
		int lastsessioninstr_;

		//! Session password to set
		/**
		 * If not empty, the session password is set right after joining
		 * a session that we are hosting.
		 */
		QString setsessionpassword_;

		//! Administrator password to send
		/**
		 * The administrator password to send in reply of authentication
		 * request, that was raised by the Authenticate message sent by us.
		 */
		QString sendadminpassword_;

		//! Is the user logged in
		/**
		 * This is set to true when the user has logged in to the server
		 * and can start sending instructions and join commands and such.
		 */
		bool loggedin_;
};

}

#endif

