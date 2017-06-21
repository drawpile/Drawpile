/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
#include <QSslError>
#include <QFileInfo>

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
 *
 * In some situations, the user must prompted for decisions (e.g. password is needed
 * or user must decide which session to join.) In these situations, a signal is emitted.
 */
class LoginHandler : public QObject {
	Q_OBJECT
public:
	enum Mode {HOST, JOIN};

	LoginHandler(Mode mode, const QUrl &url, QObject *parent=nullptr);

	/**
	 * @brief Set the desired user ID
	 *
	 * Only for host mode. When joining an existing session, the server assigns the user ID.
	 *
	 * @param userid
	 */
	void setUserId(int userid) { Q_ASSERT(m_mode==HOST); m_userid=userid; }

	/**
	 * @brief Set desired session ID alias
	 *
	 * Only in host mode.
	 * @param id
	 */
	void setSessionAlias(const QString &alias) { Q_ASSERT(m_mode==HOST); m_sessionAlias=alias; }

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
	 * @brief Set whether chat history should be preserved in the session
	 */
	void setPreserveChat(bool preserve) { Q_ASSERT(m_mode==HOST); m_preserveChat = preserve; }
	bool isPreservedChat() const { return m_preserveChat; }

	/**
	 * @brief Set the initial session content to upload to the server
	 * @param msgs
	 */
	void setInitialState(const QList<protocol::MessagePtr> &msgs) { Q_ASSERT(m_mode==HOST); m_initialState = msgs; }

	/**
	 * @brief Set session announcement URL
	 */
	void setAnnounceUrl(const QString &url, bool privateMode) { Q_ASSERT(m_mode==HOST); m_announceUrl = url; m_announcePrivate = privateMode; }

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
	 * This is valid only after a successful login.
	 * @return session ID
	 */
	QString sessionId() const;

	/**
	 * @brief Does the server support session persistence?
	 */
	bool supportsPersistence() const { return m_canPersist; }

public slots:
	void serverDisconnected();

	/**
	 * @brief Send password
	 *
	 * Call this in response to the needPassword signal after
	 * the user has entered their password.
	 *
	 * @param password
	 */
	void gotPassword(const QString &password);

	/**
	 * @brief Send identity
	 *
	 * Call this in response to loginNeeded signal after
	 * the user has entered their username and password.
	 *
	 * @param password
	 * @param username
	 */
	void selectIdentity(const QString &username, const QString &password);

	/**
	 * @brief Join the session with the given ID
	 *
	 * Call this to join the session the user selected after
	 * the sessionChoiceNeeded signal.
	 *
	 * @param id session ID
	 * @param needPassword if true, ask for password before joining
	 */
	void joinSelectedSession(const QString &id, bool needPassword);

	/**
	 * @brief Accept server certificate and proceed with login
	 */
	void acceptServerCertificate();

	/**
	 * @brief Abort login process
	 *
	 * Call this if a user clicks on "Cancel".
	 */
	void cancelLogin();

signals:
	/**
	 * @brief The user must enter a password to proceed
	 *
	 * This is emitted when a session is password protected.
	 * After the user has made a decision, call either
	 * gotPassword(password) to proceed or cancelLogin() to exit.
	 *
	 * @param prompt prompt text
	 */
	void passwordNeeded(const QString &prompt);

	/**
	 * @brief Login details are needeed to proceed
	 *
	 * This is emitted when a password is needed to log in,
	 * either because the account is protected or because guest
	 * logins are not allowed.
	 *
	 * Proceed by calling either selectIdentity(username, password) or cancelLogin()
	 *
	 * @param prompt prompt text
	 */
	void loginNeeded(const QString &prompt);

	/**
	 * @brief User must select which session to join
	 *
	 * Call joinSelectedSession(id, needPassword) or cancelLogin()
	 * to proceed.
	 * @param sessions
	 */
	void sessionChoiceNeeded(LoginSessionModel *sessions);

	/**
	 * @brief certificateCheckNeeded
	 *
	 * The certificate does not match the stored one, but the user may still
	 * proceed.
	 *
	 * Call acceptServerCertificate() or cancelLogin() to proceed()
	 */
	void certificateCheckNeeded(const QSslCertificate &newCert, const QSslCertificate &oldCert);

	/**
	 * @brief Server title has changed
	 * @param title new title
	 */
	void serverTitleChanged(const QString &title);

private slots:
	void failLogin(const QString &message, const QString &errorcode=QString());
	void tlsStarted();
	void tlsError(const QList<QSslError> &errors);

private:
	enum State {
		EXPECT_HELLO,
		EXPECT_STARTTLS,
		WAIT_FOR_LOGIN_PASSWORD,
		EXPECT_IDENTIFIED,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
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
	void continueTls();
	void send(const protocol::ServerCommand &cmd);
	void handleError(const QString &code, const QString &message);

	Mode m_mode;
	QUrl m_address;

	// Settings for hosting
	int m_userid;
	QString m_sessionPassword;
	QString m_sessionAlias;
	QString m_title;
	int m_maxusers;
	bool m_allowdrawing;
	bool m_layerctrllock;
	bool m_preserveChat;
	bool m_announcePrivate;
	QString m_announceUrl;
	QList<protocol::MessagePtr> m_initialState;

	// Settings for joining
	QString m_joinPassword;
	QString m_autoJoinId;

	// Process state
	TcpServer *m_server;
	State m_state;
	LoginSessionModel *m_sessions;

	QString m_selectedId;
	QString m_loggedInSessionId;

	QFileInfo m_certFile;

	// Server flags
	bool m_multisession;
	bool m_tls;
	bool m_canPersist;
	bool m_mustAuth;
	bool m_needUserPassword;
};

}

#endif
