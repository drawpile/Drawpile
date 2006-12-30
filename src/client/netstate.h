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
#ifndef NETSTATE_H
#define NETSTATE_H

#include <QObject>

namespace protocol {
	class HostInfo;
	class UserInfo;
	class Authentication;
	class Error;
};

namespace network {

class Connection;

//! Network state machine
/**
 * This class handles the state of a host connection.
 */
class HostState : public QObject {
	Q_OBJECT
	public:
		HostState(QObject *parent);

		//! Get the local user ID as assigned by the server
		int localUserId() const { return userid_; }

		//! Set network connection object to use
		void setConnection(Connection *net) { net_ = net; }

		//! Prepare to host a session
		void host(const QString& username, const QString& title,
				const QString& password);

		//! Initiate login sequence
		void login();

		//! Send a password
		void sendPassword(const QString& password);

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

		//! An error message was received from the host
		void error(const QString& message);

	private:
		//! Handle a HostInfo message
		void handleHostInfo(protocol::HostInfo *msg);

		//! Handle a UserInfo message
		void handleUserInfo(protocol::UserInfo *msg);

		//! Handle authentication request
		void handleAuthentication(protocol::Authentication *msg);

		//! Handle errors
		void handleError(protocol::Error *msg);

	private:
		Connection *net_;

		QString username_;
		QString title_;
		QString password_;

		int userid_;

		enum {LOGIN, JOIN, DRAWING} state_;
		enum {HOSTSESSION, JOINSESSION} mode_;
};

}

#endif

