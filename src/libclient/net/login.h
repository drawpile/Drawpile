// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_CLIENT_NET_LOGINHANDLER_H
#define DP_CLIENT_NET_LOGINHANDLER_H
#include "libclient/canvas/reconnectstate.h"
#include "libclient/net/message.h"
#include <QByteArray>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QSharedPointer>
#include <QSslError>
#include <QString>
#include <QUrl>
#include <QVector>

class QImage;
class QSslCertificate;

namespace net {

class LoginSessionModel;
class Server;
struct LoginHostParams;
struct LoginSession;
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

	LoginHandler(
		const QSharedPointer<const LoginHostParams> &hostParams,
		const QString &autoJoinId, const QUrl &url, quint64 redirectNonce,
		const QStringList &redirectHistory, const QJsonObject &redirectData,
		QObject *parent = nullptr);

#ifdef __EMSCRIPTEN__
	~LoginHandler() override;
#endif

	void setReconnectState(canvas::ReconnectState *reconnectState)
	{
		m_reconnectState = reconnectState;
	}

	/**
	 * @brief Set the server we're communicating with
	 * @param server
	 */
	void setServer(Server *server) { m_server = server; }

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

	bool anyMessageReceived() const { return m_messageReceived; }

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
	bool supportsPersistence() const;

	bool supportsCryptBanImEx() const { return m_supportsCryptBanImpEx; }
	bool supportsModBanImEx() const { return m_supportsModBanImpEx; }

	/**
	 * @brief Can the server receive abuse reports?
	 */
	bool supportsAbuseReports() const { return m_canReport; }

	bool mustAuth() const { return m_mustAuth; }

	bool compatibilityMode() const { return m_compatibilityMode; }

	const QStringList &userFlags() const { return m_userFlags; }

	/**
	 * @brief Did the user authenticate succesfully?
	 */
	bool isAuthenticated() const { return !m_isGuest; }

	/**
	 * Get the session flags
	 */
	const QStringList &sessionFlags() const { return m_sessionFlags; }

	const QString &joinPassword() const { return m_joinPassword; }

	bool skipCatchup() const { return m_skipCatchup; }

	const HistoryIndex &historyIndex() const { return m_historyIndex; }

public slots:
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

#ifdef __EMSCRIPTEN__
	void requestBrowserAuth();
	void cancelBrowserAuth();
	void selectBrowserAuthUsername(const QString &username);
	void browserAuthIdentified(const QString &token);
#endif

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

	void tlsStarted();
	void tlsError(const QList<QSslError> &errors);
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
		const QString &currentUsername, const QString &currentPassword,
		const QUrl &url, const QString &host, LoginMethod intent);

	/**
	 * @brief External authentication request completed
	 * @param success did the request complete successfully?
	 */
	void extAuthComplete(
		bool success, LoginMethod intent, const QString &host,
		const QString &username);

#ifdef __EMSCRIPTEN__
	void browserAuthCancelled();
#endif

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

	void redirectRequested(
		const QSharedPointer<const LoginHostParams> &hostParams,
		const QString &autoJoinId, const QUrl &url, quint64 redirectNonce,
		const QStringList &redirectHistory, const QJsonObject &redirectData,
		bool late);

	void replacedByRedirect(LoginHandler *login, bool late);

private slots:
	void
	failLogin(const QString &message, const QString &errorcode = QString());

private:
	static constexpr int MAX_REDIRECTS = 3;

	enum State {
		EXPECT_HELLO,
		EXPECT_STARTTLS,
		EXPECT_CLIENT_INFO_OK,
		EXPECT_LOOKUP_OK,
		WAIT_FOR_RULE_ACCEPTANCE,
		WAIT_FOR_LOGIN_METHOD,
		WAIT_FOR_LOGIN_PASSWORD,
		WAIT_FOR_EXTAUTH,
		EXPECT_IDENTIFIED,
		EXPECT_SESSIONLIST_TO_JOIN,
		EXPECT_SESSIONLIST_TO_HOST,
		WAIT_FOR_JOIN_PASSWORD,
		EXPECT_LOGIN_OK,
		ABORT_LOGIN,
		REDIRECTING,
	};

	net::LoginSessionModel *getOrMakeSessions();

	void send(
		const QString &cmd, const QJsonArray &args = QJsonArray(),
		const QJsonObject &kwargs = QJsonObject(), bool containsAvatar = false);

	void setState(State state);
	void expectNothing();
	void expectHello(const ServerReply &msg);
	void expectStartTls(const ServerReply &msg);
	void sendClientInfo();
	void expectClientInfoOk(const ServerReply &msg);
	void lookUpSession();
	void expectLookupOk(const ServerReply &msg);
	void handleRedirect(const QJsonObject &reply, bool late);
	void presentRules();
	void chooseLoginMethod();
	void requestLoginMethodChoice();
	void prepareToSendIdentity();
	void sendIdentity();
	void expectIdentified(const ServerReply &msg);
	void showPasswordDialog(const QString &title, const QString &text);
	void expectSessionDescriptionHost(const ServerReply &msg);
	void sendHostCommand();
	void sendInitialState();
	void expectSessionDescriptionJoin(const ServerReply &msg);
	LoginSession updateSession(const QJsonObject &js);
	bool checkSession(const LoginSession &session, bool fail);
	void sendJoinCommand();
	void expectNoErrors(const ServerReply &msg);
	bool expectLoginOk(const ServerReply &msg);
	void startTls();
	void continueTls();
	void handleError(const QString &code, const QString &message);

	QString takeAvatar();

	void updateAddressSessionId(QString sessionId);
	void updateAddressLoginMethod(LoginMethod loginMethod);

	static LoginMethod parseLoginMethod(const QString &method);
	static QString loginMethodToString(LoginMethod method);
#ifndef __EMSCRIPTEN__
	static bool looksLikeSelfSignedCertificate(
		const QSslCertificate &cert, const QList<QSslError> &errors);
#endif
	static QJsonObject makeClientInfo();
	static QString getSid();
	static QString generateTamperSid();
	static QString generateSid();
	static quint64 generateRedirectNonce();

#ifdef __EMSCRIPTEN__
	bool inBrowserAuth() const { return m_inBrowserAuth; }
	void authenticateBrowserAuth();
#else
	static constexpr bool inBrowserAuth() { return false; }
#endif

	const Mode m_mode;
	const QSharedPointer<const LoginHostParams> m_hostParams;

	QUrl m_address;
	QPixmap m_avatar;

	// Settings for hosting
	uint8_t m_userid;
	QString m_authId;

	// Settings for joining
	QString m_joinPassword;
	QString m_autoJoinId;

	QUrl m_extAuthUrl;
	QString m_extAuthGroup;
	QString m_extAuthNonce;

	// Process state
	Server *m_server;
	QPointer<canvas::ReconnectState> m_reconnectState;
	State m_state = EXPECT_HELLO;
	State m_passwordState = WAIT_FOR_LOGIN_PASSWORD;
	LoginSessionModel *m_sessions = nullptr;

	QString m_selectedId;
	QStringList m_sessionFlags;

	QFileInfo m_certFile;

	bool m_messageReceived = false;
#ifdef __EMSCRIPTEN__
	bool m_inBrowserAuth = false;
#endif

	// Server flags
	bool m_multisession = false;
	bool m_canAnyonePersist = false;
	bool m_canReport = false;
	bool m_mustAuth = false;
	bool m_needUserPassword = false;
	bool m_supportsCustomAvatars = false;
	bool m_supportsCryptBanImpEx = false;
	bool m_supportsModBanImpEx = false;
	bool m_supportsClientInfo = false;
	bool m_supportsLookup = false;
	bool m_supportsExtAuthAvatars = false;
	bool m_mayRedirect = false;
	bool m_acceptsRedirects = false;
	bool m_compatibilityMode = false;
	bool m_needSessionPassword = false;
	bool m_isGuest = true;
	bool m_skipCatchup = false;
	HistoryIndex m_historyIndex;

	quint64 m_redirectNonce = 0;
	QStringList m_redirectHistory;
	QJsonObject m_redirectData;

	QString m_ruleText;
	QString m_loginInfo;
	QVector<LoginMethod> m_loginMethods;
	QUrl m_loginExtAuthUrl;
	LoginMethod m_loginIntent = LoginHandler::LoginMethod::Unknown;
	bool m_loginFromUrl = false;

	// User flags
	QStringList m_userFlags;
};

struct LoginHostParams {
	bool remote;
	bool nsfm;
	bool keepChat;
	bool deputies;
	uint8_t userId;
	QString alias;
	QString title;
	QString password;
	QString operatorPassword;
	QStringList announcementUrls;
	QStringList bansToImport;
	QJsonArray authToImport;
	net::MessageList initialState;
};

}

#endif
