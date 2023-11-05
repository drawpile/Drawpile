// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_CLIENT_NET_LOGINHANDLER_H
#define DP_CLIENT_NET_LOGINHANDLER_H
#include "libclient/net/message.h"
#include "libshared/net/protover.h"
#include <QByteArray>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPixmap>
#include <QSslError>
#include <QString>
#include <QUrl>
#include <QVector>

class QImage;

namespace net {

class TcpServer;
class LoginSessionModel;
struct ServerCommand;
struct ServerReply;

/**
 * @brief Login process state machine
 *
 * See also LoginHandler in src/shared/server/ for the serverside implementation
 *
 * In some situations, the user must prompted for decisions (e.g. password is
 * needed or user must decide which session to join.) In these situations, a
 * signal is emitted.
 */
class LoginHandler final : public QObject {
	Q_OBJECT
public:
	enum class Mode { HostRemote, HostBuiltin, Join };
	enum class LoginMethod { Unknown, Guest, Auth, ExtAuth };

	LoginHandler(Mode mode, const QUrl &url, QObject *parent = nullptr);

	/**
	 * @brief Set the desired user ID
	 *
	 * Only for host mode. When joining an existing session, the server assigns
	 * the user ID.
	 *
	 * @param userid
	 */
	void setUserId(uint8_t userid)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_userid = userid;
	}

	/**
	 * @brief Set desired session ID alias
	 *
	 * Only in host mode.
	 * @param id
	 */
	void setSessionAlias(const QString &alias)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_sessionAlias = alias;
	}

	/**
	 * @brief Set the session password
	 *
	 * Only for host mode.
	 *
	 * @param password
	 */
	void setPassword(const QString &password)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_sessionPassword = password;
	}

	/**
	 * @brief Set the session title
	 *
	 * Only for host mode.
	 *
	 * @param title
	 */
	void setTitle(const QString &title)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_title = title;
	}

	/**
	 * @brief Set the initial session content to upload to the server
	 *
	 * Only for remote host mode.
	 *
	 * @param msgs
	 */
	void setInitialState(const net::MessageList &msgs)
	{
		Q_ASSERT(m_mode == Mode::HostRemote);
		m_initialState = msgs;
	}

	/**
	 * @brief Set session announcement URL
	 *
	 * Only for host mode.
	 */
	void setAnnounceUrl(const QString &url)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_announceUrl = url;
	}

	/**
	 * @brief Set session NSFM flag
	 *
	 * Only for host mode.
	 */
	void setNsfm(bool nsfm)
	{
		Q_ASSERT(m_mode != Mode::Join);
		m_nsfm = nsfm;
	}

	/**
	 * @brief Set the server we're communicating with
	 * @param server
	 */
	void setServer(TcpServer *server) { m_server = server; }

	/**
	 * @brief Handle a received message
	 * @param message
	 * @return true if expecting more login messages
	 */
	bool receiveMessage(const ServerReply &msg);

	/**
	 * @brief Login mode (host or join)
	 * @return
	 */
	Mode mode() const { return m_mode; }

	/**
	 * @brief Session URL
	 *
	 * The ID of the selected session is included
	 */
	const QUrl &url() const { return m_address; }

	/**
	 * @brief get the user ID assigned by the server
	 * @return user id
	 */
	uint8_t userId() const { return m_userid; }

	const QString &authId() const { return m_authId; }

	/**
	 * @brief Does the server support session persistence?
	 */
	bool supportsPersistence() const { return m_canPersist; }

	bool supportsCryptBanImEx() const { return m_supportsCryptBanImpEx; }
	bool supportsModBanImEx() const { return m_supportsModBanImpEx; }

	/**
	 * @brief Can the server receive abuse reports?
	 */
	bool supportsAbuseReports() const { return m_canReport; }

	bool compatibilityMode() const { return m_compatibilityMode; }

	/**
	 * @brief Check if the user has the given flag
	 *
	 * This is only valid after the user has identified succesfully
	 *
	 * @param flag
	 * @return
	 */
	bool hasUserFlag(const QString &flag) const;

	/**
	 * @brief Did the user authenticate succesfully?
	 */
	bool isAuthenticated() const { return !m_isGuest; }

	/**
	 * Get the session flags
	 */
	QStringList sessionFlags() const { return m_sessionFlags; }

	QString joinPassword() const { return m_joinPassword; }

public slots:
	void serverDisconnected();

	void acceptRules();

	/**
	 * @brief Send password
	 *
	 * Call this in response to the needSessionPassword signal after
	 * the user has entered their password.
	 *
	 * @param password
	 */
	void sendSessionPassword(const QString &password);

	/**
	 * @brief Select the avatar to use
	 *
	 * Call this BEFORE calling selectIdentity for the first time.
	 *
	 * @param avatar
	 */
	void selectAvatar(const QPixmap &avatar);

	/**
	 * @brief Send identity
	 *
	 * Call this in response to loginNeeded signal after
	 * the user has entered their username and password.
	 *
	 * @param password
	 * @param username
	 */
	void selectIdentity(
		const QString &username, const QString &password,
		LoginMethod intendedMethod);

	/**
	 * @brief Log in using an extauth token
	 *
	 * This is the extauth path equivalent to selectIdentity()
	 * A HTTP POST request will be made to the extauth URL with
	 * the given username and password. If authentication is successful,
	 * login will proceed with the returned token.
	 *
	 * @param
	 */
	void requestExtAuth(const QString &username, const QString &password);

	/**
	 * @brief Join the session with the given ID
	 *
	 * Call this to join the session the user selected after
	 * the sessionChoiceNeeded signal.
	 *
	 * @param id session ID
	 * @param needPassword if true, ask for password before joining
	 */
	void prepareJoinSelectedSession(
		const QString &id, bool needPassword, bool compatibilityMode,
		const QString &title, bool nsfm, bool autoJoin);

	/**
	 * @brief Actually join the session that the user selected.
	 *
	 * Call this after confirming that the user really wants to join the
	 * selected session.
	 */
	void confirmJoinSelectedSession();

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

	/**
	 * @brief Send an abuse report about a session
	 * @param id ID of the session being reported
	 * @param reason message to moderators
	 */
	void reportSession(const QString &id, const QString &reason);

signals:
	void ruleAcceptanceNeeded(const QString &ruleText);

	void loginMethodChoiceNeeded(
		const QVector<LoginMethod> &methods, const QUrl &url,
		const QUrl &extAuthUrl, const QString &loginInfo);

	void loginMethodMismatch(
		LoginMethod intent, LoginMethod method, bool extAuthFallback);

	/**
	 * @brief The user must enter a username to proceed
	 *
	 * This is emitted if no username was set in the URL.
	 *
	 * Proceed by calling selectIdentity(username, QString())
	 * (omit password at this point to attempt a guest login)
	 *
	 * @param canSelectCustomAvatar is true if the server has announced that it
	 * accepts custom avatars
	 */
	void usernameNeeded(bool canSelectCustomAvatar);

	/**
	 * @brief The user must confirm their session choice
	 *
	 * Emitted just before joining a session, used to confirm if the user really
	 * wants to join an NSFM session.
	 *
	 * @param title the session title
	 * @param nsfm if the session is marked NSFM
	 * @param autoJoin if this was an automatic join or if the user picked it
	 */
	void
	sessionConfirmationNeeded(const QString &title, bool nsfm, bool autoJoin);

	/**
	 * @brief The user must enter a password to proceed
	 *
	 * This is emitted when attempting to join a session that is password
	 * protected. After the user has made a decision, call either
	 * sendSessionPassword(password) to proceed or cancelLogin() to exit.
	 *
	 */
	void sessionPasswordNeeded();

	/**
	 * @brief Login details are needeed to proceed
	 *
	 * This is emitted when a password is needed to log in,
	 * either because the account is protected or because guest
	 * logins are not allowed.
	 *
	 * Proceed by calling either selectIdentity(username, password) or
	 * cancelLogin()
	 *
	 * @param prompt prompt text
	 */
	void loginNeeded(
		const QString &currentUsername, const QString &prompt,
		const QString &host, LoginMethod intent);

	/**
	 * @brief External authentication is needed
	 *
	 * This is similar to loginNeeded(), except an auth server at the given URL
	 * should be used to perform the authentication.
	 *
	 * Proceed by calling requestExtUath(username, password) or cancelLogin()
	 *
	 * @param url ext auth server URL
	 */
	void extAuthNeeded(
		const QString &currentUsername, const QUrl &url, const QString &host,
		LoginMethod intent);

	/**
	 * @brief External authentication request completed
	 * @param success did the request complete successfully?
	 */
	void extAuthComplete(
		bool success, LoginMethod intent, const QString &host,
		const QString &username);

	/**
	 * @brief Username and password (unless in guest mode) OK.
	 */
	void
	loginOk(LoginMethod intent, const QString &host, const QString &username);

	/**
	 * @brief Server user account password was wrong
	 */
	void badLoginPassword(
		LoginMethod intent, const QString &host, const QString &username);

	/**
	 * @brief User must select which session to join
	 *
	 * Call joinSelectedSession(id, needPassword) or cancelLogin()
	 * to proceed.
	 * @param sessions
	 */
	void sessionChoiceNeeded(LoginSessionModel *sessions);

	void badSessionPassword();

	/**
	 * @brief certificateCheckNeeded
	 *
	 * The certificate does not match the stored one, but the user may still
	 * proceed.
	 *
	 * Call acceptServerCertificate() or cancelLogin() to proceed()
	 */
	void certificateCheckNeeded(
		const QSslCertificate &newCert, const QSslCertificate &oldCert);

	/**
	 * @brief Server title has changed
	 * @param title new title
	 */
	void serverTitleChanged(const QString &title);

private slots:
	void
	failLogin(const QString &message, const QString &errorcode = QString());
	void tlsStarted();
	void tlsError(const QList<QSslError> &errors);

private:
	enum State {
		EXPECT_HELLO,
		EXPECT_STARTTLS,
		WAIT_FOR_LOGIN_PASSWORD,
		WAIT_FOR_EXTAUTH,
		EXPECT_IDENTIFIED,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
		EXPECT_LOGIN_OK,
		ABORT_LOGIN
	};

	void send(
		const QString &cmd, const QJsonArray &args = QJsonArray(),
		const QJsonObject &kwargs = QJsonObject(), bool containsAvatar = false);

	void expectNothing();
	void expectHello(const ServerReply &msg);
	void expectStartTls(const ServerReply &msg);
	void presentRules();
	void chooseLoginMethod();
	void prepareToSendIdentity();
	void sendIdentity();
	void expectIdentified(const ServerReply &msg);
	void showPasswordDialog(const QString &title, const QString &text);
	void expectSessionDescriptionHost(const ServerReply &msg);
	void sendHostCommand();
	void expectSessionDescriptionJoin(const ServerReply &msg);
	void sendJoinCommand();
	void expectNoErrors(const ServerReply &msg);
	bool expectLoginOk(const ServerReply &msg);
	void startTls();
	void continueTls();
	void handleError(const QString &code, const QString &message);

	QString takeAvatar();

	static LoginMethod parseLoginMethod(const QString &method);
	static QString loginMethodToString(LoginMethod method);
	static QJsonObject makeClientInfoKwargs();
	static QString getSid();
	static QString generateTamperSid();
	static QString generateSid();

	Mode m_mode;
	QUrl m_address;
	QPixmap m_avatar;

	// Settings for hosting
	uint8_t m_userid;
	QString m_authId;
	QString m_sessionPassword;
	QString m_sessionAlias;
	QString m_title;
	QString m_announceUrl;
	bool m_nsfm;
	net::MessageList m_initialState;

	// Settings for joining
	QString m_joinPassword;
	QString m_autoJoinId;

	QUrl m_extAuthUrl;
	QString m_extAuthGroup;
	QString m_extAuthNonce;

	// Process state
	TcpServer *m_server;
	State m_state;
	State m_passwordState;
	LoginSessionModel *m_sessions;

	QString m_selectedId;
	QStringList m_sessionFlags;

	QFileInfo m_certFile;

	// Server flags
	bool m_multisession;
	bool m_canPersist;
	bool m_canReport;
	bool m_mustAuth;
	bool m_needUserPassword;
	bool m_supportsCustomAvatars;
	bool m_supportsCryptBanImpEx;
	bool m_supportsModBanImpEx;
	bool m_supportsExtAuthAvatars;
	bool m_compatibilityMode;
	bool m_needSessionPassword;

	QString m_ruleText;
	QString m_loginInfo;
	QVector<LoginMethod> m_loginMethods;
	QUrl m_loginExtAuthUrl;
	LoginMethod m_loginIntent = LoginHandler::LoginMethod::Unknown;

	// User flags
	QStringList m_userFlags;
	bool m_isGuest;
};

}

#endif
