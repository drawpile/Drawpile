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
#ifndef NETSTATE_H
#define NETSTATE_H

#include <QObject>
#include <QHash>
#include <QList>

namespace protocol {
	class HostInfo;
	class UserInfo;
	class SessionInfo;
	class Authentication;
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

		//! Get the local user ID as assigned by the server
		int localUserId() const { return userid_; }

		//! Get the session state
		SessionState *session(int id) { return mysessions_.value(id); }

		//! Set network connection object to use
		void setConnection(Connection *net) { net_ = net; }

		//! Initiate login sequence
		void login(const QString& username);

		//! Send a password
		void sendPassword(const QString& password);

		//! Host a session
		void host(const QString& title, const QString& password,
				quint16 width, quint16 height);

		//! Join a specific session
		void join(int id);

	public slots:
		//! Get a message from the network handler object
		void receiveMessage();

	signals:
		//! A password must be requested from the user
		/**
		 * A password can be needed to log in to the server and to join
		 * a drawing session.
		 * @param session if true, the password is for a session
		 */
		void needPasword(bool session);

		//! Login sequence completed succesfully
		void loggedin();

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

		//! Session list was refreshed
		void sessionsListed();

	private slots:
		//! Join the latest session that the local user owns.
		void joinLatest();

	private:
		//! Refresh the list of sessions
		void listSessions();

		//! Handle a HostInfo message
		void handleHostInfo(protocol::HostInfo *msg);

		//! Handle a UserInfo message
		void handleUserInfo(protocol::UserInfo *msg);

		//! Handle a SessionInfo message
		void handleSessionInfo(protocol::SessionInfo *msg);

		//! Handle authentication request
		void handleAuthentication(protocol::Authentication *msg);

		//! Handle Acknowledgements
		void handleAck(protocol::Acknowledgement *msg);

		//! Handle errors
		void handleError(protocol::Error *msg);

		Connection *net_;

		QString username_;

		int userid_;

		SessionState *newsession_;
		QHash<int, SessionState*> mysessions_;
		QList<protocol::SessionInfo*> sessions_;

		bool loggedin_;
};

//! Network session state machine
/**
 * This class handles the state of a single session.
 */
class SessionState : public QObject {
	Q_OBJECT
	public:
		//! Construct a session state object
		SessionState(HostState *parent, const protocol::SessionInfo *info);

		//! Get session id
		/**
		 * @return session ID number
		 * @retval -1 if session does not yet have an ID
		 */
		int id() const { return id_; }

		//! Get the width of the board
		/**
		 * @return board width
		 */
		quint16 width() const { return width_; }

		//! Get the height of the board
		/**
		 * @return board height
		 */
		quint16 height() const { return height_; }

		//! Get the title of the board
		/**
		 * @return session title
		 */
		const QString& title() const { return title_; }

		//! Handle session specific user info
		void handleUserInfo(protocol::UserInfo *msg);

	private:
		HostState *host_;
		QString title_;
		QString password_;
		quint16 width_, height_;
		int id_;
};

}

#endif

