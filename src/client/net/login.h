/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include "../shared/net/message.h"

#include <QString>
#include <QUrl>
#include <QObject>
#include <QPointer>
#include <QMessageBox>
#include <QSslError>

namespace dialogs {
	class SelectSessionDialog;
	class LoginDialog;
}

namespace protocol {
	struct ServerCommand;
	struct ServerReply;
}

namespace net {

class TcpServer;
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
	void setUserId(int userid) { Q_ASSERT(m_mode==HOST); m_userid=userid; }

	/**
	 * @brief Set desired session ID
	 *
	 * Only in host mode. Use URL path when joining.
	 * @param id
	 */
	void setSessionId(const QString &id) { Q_ASSERT(m_mode==HOST); m_hostSessionId=id; }

	/**
	 * @brief Set the session password
	 *
	 * Only for host mode.
	 *
	 * @param password
	 */
	void setPassword(const QString &password) { Q_ASSERT(m_mode==HOST); m_sessionPassword=password; }

	/**
	 * @brief Set the session title
	 *
	 * Only for host mode.
	 *
	 * @param title
	 */
	void setTitle(const QString &title) { Q_ASSERT(m_mode==HOST); m_title=title; }

	/**
	 * @brief Set the maximum number of users the session will accept
	 *
	 * Only for host mode.
	 *
	 * @param maxusers
	 */
	void setMaxUsers(int maxusers) { Q_ASSERT(m_mode==HOST); m_maxusers = maxusers; }

	/**
	 * @brief Set whether new users should be locked by default
	 *
	 * Only for host mode.
	 *
	 * @param allowdrawing
	 */
	void setAllowDrawing(bool allowdrawing) { Q_ASSERT(m_mode==HOST); m_allowdrawing = allowdrawing; }

	/**
	 * @brief Set whether layer controls should be locked to operators only by default
	 *
	 * Only for host mode.
	 *
	 * @param layerlock
	 */
	void setLayerControlLock(bool layerlock) { Q_ASSERT(m_mode==HOST); m_layerctrllock = layerlock; }

	/**
	 * @brief Set whether the session should be persistent
	 *
	 * Only for host mode. Whether this option actually gets set depends on whether the server
	 * supports persistent sessions.
	 *
	 * @param persistent
	 */
	void setPersistentSessions(bool persistent) { Q_ASSERT(m_mode==HOST); m_requestPersistent = persistent; }

	/**
	 * @brief Set whether chat history should be preserved in the session
	 */
	void setPreserveChat(bool preserve) { Q_ASSERT(m_mode==HOST); m_preserveChat = preserve; }

	/**
	 * @brief Set the initial session content to upload to the server
	 * @param msgs
	 */
	void setInitialState(const QList<protocol::MessagePtr> &msgs) { m_initialState = msgs; }

	/**
	 * @brief Set session announcement URL
	 */
	void setAnnounceUrl(const QString &url) { m_announceUrl = url; }

	/**
	 * @brief Set the server we're communicating with
	 * @param server
	 */
	void setServer(TcpServer *server) { m_server = server; }

	/**
	 * @brief Handle a received message
	 * @param message
	 */
	void receiveMessage(protocol::MessagePtr message);

	/**
	 * @brief Login mode (host or join)
	 * @return
	 */
	Mode mode() const { return m_mode; }

	/**
	 * @brief Server URL
	 * @return
	 */
	const QUrl &url() const { return m_address; }

	/**
	 * @brief get the user ID assigned by the server
	 * @return user id
	 */
	int userId() const { return m_userid; }

	/**
	 * @brief get the ID of the session.
	 *
	 * This is set to the actual session ID after login succeeds.
	 * @return session ID
	 */
	QString sessionId() const;

public slots:
	void serverDisconnected();

private slots:
	void joinSelectedSession(const QString &id, bool needPassword);
	void selectIdentity(const QString &password, const QString &username);
	void cancelLogin();
	void failLogin(const QString &message, const QString &errorcode=QString());
	void passwordSet(const QString &password);
	void tlsStarted();
	void tlsError(const QList<QSslError> &errors);
	void tlsAccepted();

private:
	enum State {
		EXPECT_HELLO,
		EXPECT_STARTTLS,
		WAIT_FOR_LOGIN_PASSWORD,
		EXPECT_IDENTIFIED,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
		WAIT_FOR_HOST_PASSWORD,
		EXPECT_LOGIN_OK,
		ABORT_LOGIN
	};

	void expectNothing(const protocol::ServerReply &msg);
	void expectHello(const protocol::ServerReply &msg);
	void expectStartTls(const protocol::ServerReply &msg);
	void prepareToSendIdentity();
	void sendIdentity();
	void expectIdentified(const protocol::ServerReply &msg);
	void showPasswordDialog(const QString &title, const QString &text);
	void expectSessionDescriptionHost(const protocol::ServerReply &msg);
	void sendHostCommand();
	void expectSessionDescriptionJoin(const protocol::ServerReply &msg);
	void sendJoinCommand();
	void expectNoErrors(const protocol::ServerReply &msg);
	void expectLoginOk(const protocol::ServerReply &msg);
	void startTls();
	void send(const protocol::ServerCommand &cmd);
	void handleError(const QString &code, const QString &message);

	Mode m_mode;
	QUrl m_address;
	QWidget *_widgetParent;

	// session properties for hosting
	int m_userid;
	QString m_sessionPassword;
	QString m_title;
	int m_maxusers;
	bool m_allowdrawing;
	bool m_layerctrllock;
	bool m_requestPersistent;
	bool m_preserveChat;
	QString m_announceUrl;
	QList<protocol::MessagePtr> m_initialState;

	// Process state
	TcpServer *m_server;
	State m_state;
	LoginSessionModel *m_sessions;

	QString m_hostPassword;
	QString m_joinPassword;
	QString m_hostSessionId;
	QString m_selectedId;
	QString m_loggedInSessionId;

	QString m_autoJoinId;

	QPointer<dialogs::SelectSessionDialog> _selectorDialog;
	QPointer<dialogs::LoginDialog> _passwordDialog;
	QPointer<QMessageBox> _certDialog;

	// Server flags
	bool m_multisession;
	bool m_tls;
	bool m_canAuth;
	bool m_mustAuth;
	bool m_needUserPassword;
	bool m_needHostPassword;
};

}

#endif
