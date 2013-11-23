/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_CLIENT_NET_LOGINHANDLER_H
#define DP_CLIENT_NET_LOGINHANDLER_H

#include <QString>
#include <QUrl>
#include <QObject>

#include "../shared/net/message.h"

namespace net {

class Server;

/**
 * Server login process state machine
 */
class LoginHandler : public QObject {
	Q_OBJECT
public:
	enum Mode {HOST, JOIN};

	LoginHandler(Mode mode, const QUrl &url)
		: QObject(0), _mode(mode), _address(url), _state(0) { }

	/**
	 * @brief Set the desired user ID. Only for host mode.
	 * @param userid
	 */
	void setUserId(int userid) { Q_ASSERT(_mode==HOST); _userid=userid; }

	/**
	 * @brief Set the session password. Only for host mode.
	 * @param password
	 */
	void setPassword(const QString &password) { Q_ASSERT(_mode==HOST); _password=password; }

	/**
	 * @brief Set the session title. Only for host mode.
	 * @param title
	 */
	void setTitle(const QString &title) { Q_ASSERT(_mode==HOST); _title=title; }

	/**
	 * @brief Set the server we're communicating with
	 * @param server
	 */
	void setServer(Server *server) { _server = server; }

	/**
	 * @brief Handle a received message
	 * @param message
	 */
	void receiveMessage(protocol::MessagePtr message);

	Mode mode() const { return _mode; }

	const QUrl &url() const { return _address; }

	/**
	 * @brief get the user ID assigned by the server
	 * @return user id
	 */
	int userId() const { return _userid; }

private:
	void expectHello(const QString &msg);
	void expectPasswordResponse(const QString &msg);
	void expectLoginOk(const QString &msg);
	void sendLogin();

	Mode _mode;
	QUrl _address;

	// session properties for hosting
	int _userid;
	QString _password;
	QString _title;
	Server *_server;

	int _state;
	bool _requirepass;
};

}

#endif
