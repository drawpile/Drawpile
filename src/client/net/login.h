/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef DP_CLIENT_NET_LOGINHANDLER_H
#define DP_CLIENT_NET_LOGINHANDLER_H

#include <QString>
#include <QUrl>
#include <QObject>
#include <QPointer>
#include <QInputDialog>

#include "../shared/net/message.h"

namespace dialogs {
	class SelectSessionDialog;
}

namespace net {

class Server;
class LoginSessionModel;

/**
 * @brief Login process state machine
 *
 * See also LoginHandler in src/shared/server/ for the serverside implementation
 */
class LoginHandler : public QObject {
	Q_OBJECT
public:
	enum Mode {HOST, JOIN};

	LoginHandler(Mode mode, const QUrl &url, QWidget *parent=0);

	/**
	 * @brief Set the desired user ID
	 *
	 * Only for host mode. When joining an existing session, the server assigns the user ID.
	 *
	 * @param userid
	 */
	void setUserId(int userid) { Q_ASSERT(_mode==HOST); _userid=userid; }

	/**
	 * @brief Set the session password
	 *
	 * Only for host mode.
	 *
	 * @param password
	 */
	void setPassword(const QString &password) { Q_ASSERT(_mode==HOST); _sessionPassword=password; }

	/**
	 * @brief Set the session title
	 *
	 * Only for host mode.
	 *
	 * @param title
	 */
	void setTitle(const QString &title) { Q_ASSERT(_mode==HOST); _title=title; }

	/**
	 * @brief Set the maximum number of users the session will accept
	 *
	 * Only for host mode.
	 *
	 * @param maxusers
	 */
	void setMaxUsers(int maxusers) { Q_ASSERT(_mode==HOST); _maxusers = maxusers; }

	/**
	 * @brief Set whether new users should be locked by default
	 *
	 * Only for host mode.
	 *
	 * @param allowdrawing
	 */
	void setAllowDrawing(bool allowdrawing) { Q_ASSERT(_mode==HOST); _allowdrawing = allowdrawing; }

	/**
	 * @brief Set whether layer controls should be locked to operators only by default
	 *
	 * Only for host mode.
	 *
	 * @param layerlock
	 */
	void setLayerControlLock(bool layerlock) { Q_ASSERT(_mode==HOST); _layerctrllock = layerlock; }

	/**
	 * @brief Set whether the session should be persistent
	 *
	 * Only for host mode. Whether this option actually gets set depends on whether the server
	 * supports persistent sessions.
	 *
	 * @param persistent
	 */
	void setPersistentSessions(bool persistent) { Q_ASSERT(_mode==HOST); _requestPersistent = persistent; }

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

	/**
	 * @brief Login mode (host or join)
	 * @return
	 */
	Mode mode() const { return _mode; }

	/**
	 * @brief Server URL
	 * @return
	 */
	const QUrl &url() const { return _address; }

	/**
	 * @brief get the user ID assigned by the server
	 * @return user id
	 */
	int userId() const { return _userid; }

public slots:
	void serverDisconnected();

private slots:
	void joinSelectedSession(int id, bool needPassword);
	void cancelLogin();
	void passwordSet();

private:
	enum State {
		EXPECT_HELLO,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
		WAIT_FOR_HOST_PASSWORD,
		EXPECT_LOGIN_OK
	};

	void expectHello(const QString &msg);
	void showPasswordDialog(const QString &title, const QString &text);
	void expectSessionDescriptionHost(const QString &msg);
	void sendHostCommand();
	void expectSessionDescriptionJoin(const QString &msg);
	void sendJoinCommand();
	void expectNoErrors(const QString &msg);
	void expectLoginOk(const QString &msg);
	void send(const QString &message);

	Mode _mode;
	QUrl _address;
	QWidget *_widgetParent;

	// session properties for hosting
	int _userid;
	QString _sessionPassword;
	QString _title;
	int _maxusers;
	bool _allowdrawing;
	bool _layerctrllock;
	bool _requestPersistent;

	// Process state
	Server *_server;
	State _state;
	LoginSessionModel *_sessions;

	QString _hostPassword;
	QString _joinPassword;
	int _selectedId;

	QPointer<dialogs::SelectSessionDialog> _selectorDialog;
	QPointer<QInputDialog> _passwordDialog;

	// Server flags
	bool _multisession;

};

}

#endif
