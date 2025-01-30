// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/login.h"
#include "cmake-config/config.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/server.h"
#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libshared/net/protover.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/paths.h"
#include <QBuffer>
#include <QDebug>
#include <QFile>
#include <QHostAddress>
#include <QImage>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSslSocket>
#include <QStringList>
#include <QUrlQuery>
#include <QUuid>
#include <dpcommon/platform_qt.h>
#include <utility>
#ifdef __EMSCRIPTEN__
#	include "libclient/wasmsupport.h"
#endif

Q_LOGGING_CATEGORY(lcDpLogin, "net.drawpile.login", QtWarningMsg)

namespace {

enum CertLocation { KNOWN_HOSTS, TRUSTED_HOSTS };
QFileInfo getCertFile(CertLocation location, const QString &hostname)
{
	QString locationpath;
	if(location == KNOWN_HOSTS)
		locationpath = "known-hosts/";
	else
		locationpath = "trusted-hosts/";

	return QFileInfo(
		utils::paths::writablePath(locationpath, hostname + ".pem"));
}

}

namespace net {

LoginHandler::LoginHandler(Mode mode, const QUrl &url, QObject *parent)
	: QObject(parent)
	, m_mode(mode)
	, m_address(url)
{
	m_sessions = new LoginSessionModel(this);

	// Automatically join a session if the ID is included in the URL, either in
	// the path or in the "session" query parameter. WebSocket URLs only support
	// the latter, since they need the path to actually make a connection.
	QString path;
	if(!m_address.scheme().startsWith(
		   QStringLiteral("ws"), Qt::CaseInsensitive)) {
		path = m_address.path();
	}
	if(path.isEmpty()) {
		path = QUrlQuery(url).queryItemValue(QStringLiteral("session"));
	}
	m_autoJoinId = net::Server::extractAutoJoinId(path);
}

#ifdef __EMSCRIPTEN__
LoginHandler::~LoginHandler()
{
	browser::cancelLoginModal(this);
}
#endif

bool LoginHandler::receiveMessage(const ServerReply &msg)
{
	if(lcDpLogin().isDebugEnabled()) {
		qCDebug(lcDpLogin) << "login <--" << msg.reply;
	}
	m_messageReceived = true;

	// Overall, the login process is:
	// 1. wait for server greeting
	// 2. Upgrade to secure connection (if available)
	// 3. Send client info (if supported)
	// 4. Look up session (if supported)
	// 5. Authenticate user (or do guest login)
	// 6. wait for session list
	// 7. wait for user to finish typing join password if needed
	// 8. send host/join command
	// 9. wait for OK

	if(msg.type == ServerReply::ReplyType::Error) {
		// The server disconnects us right after sending the error message
		handleError(msg.reply["code"].toString(), msg.message);
		return true;

	} else if(
		msg.type != ServerReply::ReplyType::Login &&
		msg.type != ServerReply::ReplyType::Result) {
		qCWarning(lcDpLogin) << "Login error: got reply type" << int(msg.type)
							 << "when expected LOGIN, RESULT or ERROR";
		failLogin(tr("Invalid state"));

		return true;
	}

	switch(m_state) {
	case EXPECT_HELLO:
		expectHello(msg);
		break;
	case EXPECT_STARTTLS:
		expectStartTls(msg);
		break;
	case EXPECT_CLIENT_INFO_OK:
		expectClientInfoOk(msg);
		break;
	case EXPECT_LOOKUP_OK:
		expectLookupOk(msg);
		break;
	case WAIT_FOR_LOGIN_PASSWORD:
	case WAIT_FOR_EXTAUTH:
		expectNothing();
		break;
	case EXPECT_IDENTIFIED:
		expectIdentified(msg);
		break;
	case EXPECT_SESSIONLIST_TO_JOIN:
		expectSessionDescriptionJoin(msg);
		break;
	case EXPECT_SESSIONLIST_TO_HOST:
		expectSessionDescriptionHost(msg);
		break;
	case WAIT_FOR_JOIN_PASSWORD:
	case EXPECT_LOGIN_OK:
		return expectLoginOk(msg);
		break;
	case ABORT_LOGIN: /* ignore messages in this state */
		break;
	}

	return true;
}

bool LoginHandler::supportsPersistence() const
{
	return m_canAnyonePersist ||
		   m_userFlags.contains(QStringLiteral("PERSIST"));
}

void LoginHandler::setState(State state)
{
	qCDebug(lcDpLogin, "Set state to %d", int(state));
	m_state = state;
}

void LoginHandler::expectNothing()
{
	qCWarning(lcDpLogin, "Got login message while not expecting anything!");
	failLogin(tr("Incompatible server"));
}

void LoginHandler::expectHello(const ServerReply &msg)
{
	if(msg.type != ServerReply::ReplyType::Login) {
		qCWarning(lcDpLogin)
			<< "Login error. Greeting type is not LOGIN:" << int(msg.type);
		failLogin(tr("Incompatible server"));
		return;
	}

	// Server protocol version must match ours
	const int serverVersion = msg.reply["version"].toInt();
	if(serverVersion != protocol::ProtocolVersion::current().serverVersion()) {
		failLogin(tr("Server is for a different Drawpile version!"));
		return;
	}

	// Parse server capability flags
	const QJsonArray flags = msg.reply["flags"].toArray();

	m_mustAuth = false;
	m_needUserPassword = false;
	m_canAnyonePersist = false;
	m_canReport = false;

	bool startTls = false;

	for(const QJsonValue &flag : flags) {
		if(flag == QStringLiteral("MULTI")) {
			m_multisession = true;
		} else if(flag == QStringLiteral("TLS")) {
			startTls = true;
		} else if(flag == QStringLiteral("SECURE")) {
			// Changed in 2.1.9 (although in practice we've always done this):
			// this flag is implied by TLS
		} else if(flag == QStringLiteral("PERSIST")) {
			m_canAnyonePersist = true;
		} else if(flag == QStringLiteral("NOGUEST")) {
			m_mustAuth = true;
		} else if(flag == QStringLiteral("REPORT")) {
			m_canReport = true;
		} else if(flag == QStringLiteral("AVATAR")) {
			m_supportsCustomAvatars = true;
		} else if(flag == QStringLiteral("CBANIMPEX")) {
			m_supportsCryptBanImpEx = true;
		} else if(flag == QStringLiteral("MBANIMPEX")) {
			m_supportsModBanImpEx = true;
		} else if(flag == QStringLiteral("LOOKUP")) {
			m_supportsLookup = true;
		} else if(flag == QStringLiteral("CINFO")) {
			m_supportsClientInfo = true;
		} else {
			qCWarning(lcDpLogin) << "Unknown server capability:" << flag;
		}
	}

	// Server rules, if present.
	m_ruleText = msg.reply[QStringLiteral("rules")].toString().trimmed();

	// The server ideally will provide a URL to get information about how to
	// register an account and such. This isn't necessarily present, nor is it
	// necessarily a valid URL because a human will have put it in there.
	m_loginInfo = msg.reply[QStringLiteral("info")].toString().trimmed();

	// Extract available login methods, if the server provides this information.
	m_loginMethods.clear();
	m_loginExtAuthUrl.clear();
	if(msg.reply.contains(QStringLiteral("methods"))) {
		QString action = m_mode == Mode::Join ? QStringLiteral("join")
											  : QStringLiteral("host");
		QJsonObject methods = msg.reply[QStringLiteral("methods")].toObject();

		std::pair<QString, LoginMethod> knownMethods[] = {
			{QStringLiteral("guest"), LoginMethod::Guest},
			{QStringLiteral("auth"), LoginMethod::Auth},
			{QStringLiteral("extauth"), LoginMethod::ExtAuth},
		};
		for(auto [key, method] : knownMethods) {
			QJsonValue value = methods[key];
			if(value[QStringLiteral("actions")].toArray().contains(action)) {
				if(method == LoginMethod::ExtAuth) {
					QString rawUrl = value[QStringLiteral("url")].toString();
					m_loginExtAuthUrl = rawUrl;
					if(m_loginExtAuthUrl.isValid()) {
						m_loginMethods.append(method);
					} else {
						qCWarning(
							lcDpLogin, "Invalid ext-auth URL '%s': %s",
							qUtf8Printable(rawUrl),
							qUtf8Printable(m_loginExtAuthUrl.errorString()));
					}
				} else {
					m_loginMethods.append(method);
				}
			}
		}

		if(m_loginMethods.isEmpty()) {
			if(m_mode == Mode::Join) {
				failLogin(tr("This server doesn't provide a way to log in for "
							 "joining a session!"));
			} else {
				failLogin(tr("This server doesn't provide a way to log in for "
							 "hosting a session!"));
			}
			return;
		}
	}

	// Start secure mode if possible
	if(startTls) {
		if(m_server->hasSslSupport()) {
			setState(EXPECT_STARTTLS);
			send("startTls");
		} else {
			failLogin(tr("Server expects STARTTLS on unsupported socket."));
		}
	} else {
		// If this is a trusted host, it should always be in secure mode
		if(getCertFile(TRUSTED_HOSTS, m_address.host()).exists()) {
			failLogin(tr(
				"Secure mode not enabled on a host with pinned certificate!"));
			return;
		}

		sendClientInfo();
	}
}

void LoginHandler::expectStartTls(const ServerReply &msg)
{
	if(msg.reply["startTls"].toBool()) {
		startTls();

	} else {
		qCWarning(lcDpLogin)
			<< "Login error. Expected startTls, got:" << msg.reply;
		failLogin(tr("Incompatible server"));
	}
}

void LoginHandler::sendSessionPassword(const QString &password)
{
	if(m_state == WAIT_FOR_JOIN_PASSWORD) {
		m_joinPassword = password;
		sendJoinCommand();

	} else {
		// shouldn't happen...
		qCWarning(
			lcDpLogin, "sendSessionPassword() in invalid state (%d)", m_state);
	}
}

void LoginHandler::sendClientInfo()
{
	if(m_supportsClientInfo) {
		setState(EXPECT_CLIENT_INFO_OK);
		send(
			QStringLiteral("cinfo"),
			{m_address.host(QUrl::ComponentFormattingOption::FullyDecoded)
				 .toCaseFolded(),
			 makeClientInfo()});
	} else {
		lookUpSession();
	}
}

void LoginHandler::expectClientInfoOk(const ServerReply &msg)
{
	if(msg.type == ServerReply::ReplyType::Result &&
	   msg.reply.contains(QStringLiteral("cinfo"))) {
		QJsonObject cinfo = msg.reply.value(QStringLiteral("cinfo")).toObject();
		QJsonValue browserValue = cinfo.value(QStringLiteral("browser"));
		if(browserValue.isBool()) {
			m_server->setBrowser(browserValue.toBool());
		}
		lookUpSession();
	} else {
		failLogin(tr("Failed to retrieve server info"));
	}
}

void LoginHandler::lookUpSession()
{
	if(m_supportsLookup) {
		setState(EXPECT_LOOKUP_OK);
		QJsonArray args;
		if(m_mode == Mode::Join) {
			args.append(m_autoJoinId);
		}
		send("lookup", args);
	} else {
		presentRules();
	}
}

void LoginHandler::expectLookupOk(const ServerReply &msg)
{
	if(msg.type == ServerReply::ReplyType::Result) {
		QString lookup = msg.reply[QStringLiteral("lookup")].toString();
		if(m_mode == Mode::Join && lookup == QStringLiteral("join")) {
			QJsonObject js = msg.reply[QStringLiteral("session")].toObject();
			bool haveSession = !js.isEmpty();
			bool expectSession = !m_autoJoinId.isEmpty();
			if(haveSession && expectSession) {
				LoginSession session = updateSession(js);
				if(checkSession(session, true)) {
					presentRules();
				}
				return;
			} else if(!haveSession && !expectSession) {
				presentRules();
				return;
			}
		} else if(lookup == QStringLiteral("host")) {
			presentRules();
			return;
		}
	}
	failLogin(tr("Session lookup failed"));
}

void LoginHandler::presentRules()
{
	if(m_ruleText.isEmpty()) {
		acceptRules(); // Nothing to present, skip.
	} else {
		emit ruleAcceptanceNeeded(m_ruleText);
	}
}

void LoginHandler::acceptRules()
{
	chooseLoginMethod();
}

void LoginHandler::chooseLoginMethod()
{
	if(m_loginMethods.isEmpty()) {
		prepareToSendIdentity();
	} else {
		emit loginMethodChoiceNeeded(
			m_loginMethods, m_address, m_loginExtAuthUrl, m_loginInfo);
	}
}

void LoginHandler::prepareToSendIdentity()
{
	m_passwordState = WAIT_FOR_LOGIN_PASSWORD;
	if(m_address.userName().isEmpty()) {
		emit usernameNeeded(m_supportsCustomAvatars);

	} else if(m_mustAuth || m_needUserPassword) {
		setState(WAIT_FOR_LOGIN_PASSWORD);

		QString prompt;
		if(m_mustAuth)
			prompt = tr("This server does not allow guest logins");
		else
			prompt = tr("Password needed to log in as \"%1\"")
						 .arg(m_address.userName());

		emit loginNeeded(
			m_address.userName(), prompt, m_address.host(), m_loginIntent);

	} else {
		sendIdentity();
	}
}

void LoginHandler::selectAvatar(const QPixmap &avatar)
{
	m_avatar = avatar;
}

void LoginHandler::selectIdentity(
	const QString &username, const QString &password,
	LoginMethod intendedMethod)
{
	m_address.setUserName(username);
	m_address.setPassword(password);
	m_loginIntent = intendedMethod;
	sendIdentity();
}

void LoginHandler::sendIdentity()
{
	QJsonArray args;
	QJsonObject kwargs;

	args << m_address.userName();

	if(!m_address.password().isEmpty()) {
		args << m_address.password();
	}

	if(m_loginIntent != LoginHandler::LoginMethod::Unknown) {
		kwargs[QStringLiteral("intent")] = loginMethodToString(m_loginIntent);
	}

	QString avatar = takeAvatar();
	if(!avatar.isEmpty()) {
		kwargs["avatar"] = avatar;
	}

	setState(EXPECT_IDENTIFIED);
	send("ident", args, kwargs, !avatar.isEmpty());
}

void LoginHandler::requestExtAuth(
	const QString &username, const QString &password)
{
	// Construct request body
	QJsonObject o;
	o["username"] = username;
	o["password"] = password;
	if(!m_extAuthGroup.isEmpty())
		o["group"] = m_extAuthGroup;
	o["nonce"] = m_extAuthNonce;
	if(m_supportsExtAuthAvatars)
		o["avatar"] = true;
	o["s"] = getSid();

	// Send request
	QNetworkRequest req(m_extAuthUrl);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QNetworkReply *reply =
		networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	connect(reply, &QNetworkReply::finished, this, [reply, this]() {
		reply->deleteLater();

		if(reply->error() != QNetworkReply::NoError) {
			failLogin(tr("Auth server error: %1").arg(reply->errorString()));
			return;
		}
		QJsonParseError error;
		const QJsonDocument doc =
			QJsonDocument::fromJson(reply->readAll(), &error);
		if(error.error != QJsonParseError::NoError) {
			failLogin(tr("Auth server error: %1").arg(error.errorString()));
			return;
		}

		const QJsonObject obj = doc.object();
		const QString status = obj["status"].toString();
		if(status == "auth") {
			setState(EXPECT_IDENTIFIED);
			send("ident", {m_address.userName()}, {{"extauth", obj["token"]}});
			emit extAuthComplete(
				true, m_loginIntent, m_address.host(), m_address.userName());

		} else if(status == "badpass") {
			qCWarning(lcDpLogin, "Incorrect ext-auth password");
			emit extAuthComplete(
				false, m_loginIntent, m_address.host(), m_address.userName());

		} else if(status == "outgroup") {
			qCWarning(lcDpLogin, "Ext-auth error: group membership needed");
			failLogin(tr("Group membership needed"));

		} else {
			failLogin(tr("Unexpected ext-auth response: %1").arg(status));
		}
	});
}

#ifdef __EMSCRIPTEN__
void LoginHandler::requestBrowserAuth()
{
	if(!inBrowserAuth()) {
		m_inBrowserAuth = true;
		browser::showLoginModal(this);
	}
}

void LoginHandler::cancelBrowserAuth()
{
	if(inBrowserAuth()) {
		browser::cancelLoginModal(this);
		m_inBrowserAuth = false;
		emit browserAuthCancelled();
	}
}

void LoginHandler::selectBrowserAuthUsername(const QString &username)
{
	if(inBrowserAuth()) {
		m_address.setUserName(username);
		m_address.setPassword(QString());
		m_loginIntent = LoginMethod::ExtAuth;
		sendIdentity();
	}
}

void LoginHandler::authenticateBrowserAuth()
{
	QJsonObject o;
	o[QStringLiteral("nonce")] = m_extAuthNonce;
	o[QStringLiteral("s")] = getSid();
	if(!m_extAuthGroup.isEmpty()) {
		o[QStringLiteral("group")] = m_extAuthGroup;
	}
	if(m_supportsExtAuthAvatars) {
		o[QStringLiteral("avatar")] = true;
	}
	QByteArray payload = QJsonDocument(o).toJson(QJsonDocument::Compact);
	browser::authenticate(this, payload);
}

void LoginHandler::browserAuthIdentified(const QString &token)
{
	if(inBrowserAuth()) {
		m_inBrowserAuth = false;
		setState(EXPECT_IDENTIFIED);
		send("ident", {m_address.userName()}, {{"extauth", token}});
		emit extAuthComplete(
			true, m_loginIntent, m_address.host(), m_address.userName());
	}
}
#endif

void LoginHandler::expectIdentified(const ServerReply &msg)
{
	const QString state = msg.reply[QStringLiteral("state")].toString();

	if(state == QStringLiteral("intentMismatch")) {
		// The user wanted to log in using a way that doesn't match the server's
		// account situation. For example, they want to log in as a guest, but
		// the username is taken.
		LoginMethod intent =
			parseLoginMethod(msg.reply[QStringLiteral("intent")].toString());
		LoginMethod method =
			parseLoginMethod(msg.reply[QStringLiteral("method")].toString());
		bool extAuthFallback =
			msg.reply[QStringLiteral("extauthfallback")].toBool();
		// The intent sent back by the server must be the same we gave it, the
		// intents must actually be different and our own intent can't have been
		// unset. An ext-auth fallback is only valid from an ext-auth intent. An
		// unknown intent sent back by the server is not invalid, that could
		// just mean we don't know the authentication method.
		bool isValid = intent == m_loginIntent && intent != method &&
					   intent != LoginMethod::Unknown &&
					   (!extAuthFallback || intent == LoginMethod::ExtAuth) &&
					   !inBrowserAuth();
		if(isValid) {
			emit loginMethodMismatch(intent, method, extAuthFallback);
		} else {
			failLogin(tr("Invalid ident intent response."));
		}
		return;
	}

	if(state == QStringLiteral("needPassword") && !inBrowserAuth()) {
		// Looks like guest logins are not possible
		m_needUserPassword = true;
		prepareToSendIdentity();
		return;
	}

	if(state == QStringLiteral("needExtAuth")) {
		// External authentication needed for this username
		setState(WAIT_FOR_EXTAUTH);
		m_extAuthUrl = msg.reply["extauthurl"].toString();
		m_supportsExtAuthAvatars = msg.reply["avatar"].toBool();

		if(!m_extAuthUrl.isValid()) {
			qCWarning(
				lcDpLogin, "Invalid ext-auth URL: %s",
				qPrintable(msg.reply["extauthurl"].toString()));
			failLogin(tr("Server misconfiguration: invalid ext-auth URL"));
			return;
		} else if(
			m_extAuthUrl.scheme() != "http" &&
			m_extAuthUrl.scheme() != "https") {
			qCWarning(
				lcDpLogin, "Unsupported ext-auth URL: %s",
				qPrintable(msg.reply["extauthurl"].toString()));
			failLogin(tr("Unsupported ext-auth URL scheme"));
			return;
		} else if(
			m_loginExtAuthUrl.isValid() && m_extAuthUrl != m_loginExtAuthUrl) {
			qCWarning(
				lcDpLogin,
				"Ext-auth URL mismatch, got '%s' at login, but '%s' later",
				qUtf8Printable(m_loginExtAuthUrl.toString()),
				qUtf8Printable(m_extAuthUrl.toString()));
			failLogin(tr("Server reported two different ext-auth URLs"));
			return;
		}

		m_extAuthGroup = msg.reply["group"].toString();
		m_extAuthNonce = msg.reply["nonce"].toString();

#ifdef __EMSCRIPTEN__
		if(inBrowserAuth()) {
			authenticateBrowserAuth();
			return;
		}
#endif

		emit extAuthNeeded(
			m_address.userName(), m_extAuthUrl, m_address.host(),
			m_loginIntent);
		return;
	}

	if(state != QStringLiteral("identOk")) {
		qCWarning(lcDpLogin) << "Expected identOk state, got" << state;
		failLogin(tr("Invalid state"));
		return;
	}

	setState(
		m_mode == Mode::Join ? EXPECT_SESSIONLIST_TO_JOIN
							 : EXPECT_SESSIONLIST_TO_HOST);
	m_passwordState = WAIT_FOR_JOIN_PASSWORD;
	m_isGuest = msg.reply["guest"].toBool();
	for(const QJsonValue f : msg.reply["flags"].toArray()) {
		m_userFlags << f.toString().toUpper();
	}

	m_sessions->setModeratorMode(m_userFlags.contains("MOD"));

	emit loginOk(m_loginIntent, m_address.host(), m_address.userName());
	// Show session selector if in multisession mode
	// In single-session mode we can just automatically join
	// the first session we see.
	if(m_mode == Mode::Join && m_multisession) {
		emit sessionChoiceNeeded(m_sessions);
	}
}

void LoginHandler::expectSessionDescriptionHost(const ServerReply &msg)
{
	Q_ASSERT(m_mode != Mode::Join);

	if(msg.type == ServerReply::ReplyType::Login) {
		// We don't care about existing sessions when hosting a new one,
		// but the reply means we can go ahead
		sendHostCommand();

	} else {
		qCWarning(lcDpLogin) << "Expected session list, got" << msg.reply;
		failLogin(tr("Incompatible server"));
		return;
	}
}

void LoginHandler::sendHostCommand()
{
	QJsonObject kwargs = makeClientInfo();

	if(!m_sessionAlias.isEmpty())
		kwargs["alias"] = m_sessionAlias;

	kwargs["protocol"] = protocol::ProtocolVersion::current().asString();
	kwargs["user_id"] = m_userid;
	if(!m_sessionPassword.isEmpty()) {
		kwargs["password"] = m_sessionPassword;
		m_joinPassword = m_sessionPassword;
	}

	send("host", {}, kwargs);
	setState(EXPECT_LOGIN_OK);
}

void LoginHandler::sendInitialState()
{
	m_server->sendMessages(m_initialState.count(), m_initialState.constData());
}

void LoginHandler::expectSessionDescriptionJoin(const ServerReply &msg)
{
	Q_ASSERT(m_mode != Mode::HostRemote);

	if(msg.reply.contains("sessions")) {
		for(const QJsonValue &jsv : msg.reply["sessions"].toArray()) {
			LoginSession session = updateSession(jsv.toObject());
			m_sessions->updateSession(session);
			if(!m_autoJoinId.isEmpty() &&
			   (session.id == m_autoJoinId || session.alias == m_autoJoinId)) {
				// A session ID was given as part of the URL
				if(checkSession(session, false)) {
					prepareJoinSelectedSession(
						m_autoJoinId, session.needPassword,
						session.version.compatibilityMode, session.title,
						session.nsfm, true);
				} else {
					m_autoJoinId = QString();
				}
			}
		}
	}

	if(msg.reply.contains("remove")) {
		for(const QJsonValue j : msg.reply["remove"].toArray()) {
			m_sessions->removeSession(j.toString());
		}
	}

	if(!m_multisession || (!m_autoJoinId.isEmpty() && m_supportsLookup)) {
		// Single session mode: automatically join the (only) session
		int sessionCount = m_sessions->rowCount();
		if(sessionCount > 1) {
			failLogin(tr("Got multiple sessions when only one was expected"));
		} else {
			LoginSession session = m_sessions->getFirstSession();
			if(checkSession(session, true)) {
				prepareJoinSelectedSession(
					session.id, session.needPassword,
					session.version.compatibilityMode, session.title,
					session.nsfm, true);
			}
		}
	}

	if(msg.reply.contains("title")) {
		emit serverTitleChanged(msg.reply["title"].toString());
	}
}

bool LoginHandler::checkSession(const LoginSession &session, bool fail)
{
	if(session.id.isEmpty()) {
		if(fail) {
			failLogin(tr("Session not yet started!"));
		}
		return false;
	}

	if(parentalcontrols::level() >= parentalcontrols::Level::NoJoin) {
		const auto blocked =
			session.nsfm || parentalcontrols::isNsfmTitle(session.title);
		if(blocked) {
			if(fail) {
				failLogin(tr("Blocked by parental controls"));
			}
			return false;
		}
	}

	if(session.isIncompatible()) {
		if(fail) {
			failLogin(
				tr("Session for a different Drawpile version (%1) in progress!")
					.arg(session.version.description));
		}
		return false;
	}

	return true;
}

LoginSession LoginHandler::updateSession(const QJsonObject &js)
{
	QString protocol = js["protocol"].toString();
	protocol::ProtocolVersion protoVer =
		protocol::ProtocolVersion::fromString(protocol);

	LoginSessionVersion version;
	version.future = protoVer.isFuture();
	version.past = protoVer.isPast();
	version.compatibilityMode = protoVer.isPastCompatible();
#ifdef HAVE_COMPATIBILITY_MODE
	version.compatible = protoVer.isCompatible();
#else
	version.compatible = !version.compatibilityMode && protoVer.isCompatible();
#endif
	if(!version.compatible) {
		if(version.future) {
			version.description = tr("New version");
		} else {
			version.description = protoVer.versionName();
		}

		if(version.description.isEmpty()) {
			version.description = tr("Unknown version %1").arg(protocol);
		}
	}

	LoginSession session{
		js["id"].toString(),
		js["alias"].toString(),
		js["title"].toString(),
		js["founder"].toString(),
		version,
		js["userCount"].toInt(),
		js["activeDrawingUserCount"].toInt(-1),
		js["hasPassword"].toBool(),
		js["persistent"].toBool(),
		js["closed"].toBool(),
		js["authOnly"].toBool() && m_isGuest,
		!js["allowWeb"].toBool() && m_server->isBrowser(),
		js["nsfm"].toBool(),
	};
	m_sessions->updateSession(session);
	return session;
}

void LoginHandler::expectNoErrors(const ServerReply &msg)
{
	// A "do nothing" handler while waiting for the user to enter a password
	if(msg.type == ServerReply::ReplyType::Login)
		return;

	qCWarning(lcDpLogin) << "Unexpected login message:" << msg.reply;
}

bool LoginHandler::expectLoginOk(const ServerReply &msg)
{
	if(msg.type == ServerReply::ReplyType::Login) {
		// We can still get session list updates here. They are safe to ignore.
		return true;
	}

	if(msg.reply["state"] == "join" || msg.reply["state"] == "host") {
		QJsonObject join = msg.reply[QStringLiteral("join")].toObject();
		QString sessionId = join[QStringLiteral("id")].toString();
		m_address.setPath(QStringLiteral("/") + sessionId);
		int userid = join["user"].toInt();

		if(userid < 1 || userid > 254) {
			qCWarning(lcDpLogin) << "Login error. User ID" << userid
								 << "out of supported range.";
			failLogin(tr("Incompatible server"));
			return true;
		}

		m_userid = uint8_t(userid);
		m_authId = join["authId"].toString();

		QJsonArray sessionFlags = join["flags"].toArray();
		for(const QJsonValue &val : sessionFlags) {
			if(val.isString())
				m_sessionFlags << val.toString();
		}

		m_server->loginSuccess();

		// If in host mode, send initial session settings
		if(m_mode != Mode::Join) {
			QJsonObject kwargs;

			if(m_title.isEmpty()) {
				kwargs[QStringLiteral("title")] =
					QStringLiteral("%1 Drawpile").arg(m_address.userName());
				kwargs[QStringLiteral("autotitle")] = true;
			} else {
				kwargs[QStringLiteral("title")] = m_title;
			}

			if(!m_operatorPassword.isEmpty()) {
				kwargs[QStringLiteral("opword")] = m_operatorPassword;
			}
			if(m_nsfm) {
				kwargs[QStringLiteral("nsfm")] = true;
			}
			if(m_keepChat) {
				kwargs[QStringLiteral("preserveChat")] = true;
			}
			if(m_deputies) {
				kwargs[QStringLiteral("deputies")] = true;
			}

			send("sessionconf", {}, kwargs);

			for(const QString &announceUrl : m_announceUrls) {
				m_server->sendMessage(ServerCommand::makeAnnounce(announceUrl));
			}

			int authImportCount = m_authToImport.size();
			int authImportDone = 0;
			while(authImportDone < authImportCount) {
				QJsonArray auth;
				while(auth.size() < 100 && authImportDone < authImportCount) {
					auth.append(m_authToImport[authImportDone++]);
				}
				send(QStringLiteral("auth-list"), {QJsonValue(auth)});
			}

			for(const QString &bans : m_bansToImport) {
				send(QStringLiteral("import-bans"), {bans});
			}

			// Upload initial session content. For builtin sessions, there
			// should only be a feature access levels and an undo depth message
			// to apply those from the host dialog.
			if(m_mode == Mode::HostRemote) {
				sendInitialState();
				send("init-complete");
			} else if(m_mode == Mode::HostBuiltin) {
				Q_ASSERT(m_initialState.size() == 2);
				sendInitialState();
			}
		}

		// Login phase over: any remaining messages belong to the session
		return false;

	} else {
		// Unexpected response
		qCWarning(lcDpLogin)
			<< "Login error. Unexpected response while waiting for OK:"
			<< msg.reply;
		failLogin(tr("Incompatible server"));
	}

	return true;
}

void LoginHandler::prepareJoinSelectedSession(
	const QString &id, bool needPassword, bool compatibilityMode,
	const QString &title, bool nsfm, bool autoJoin)
{
	Q_ASSERT(!id.isEmpty());
	m_selectedId = id;
	m_compatibilityMode = compatibilityMode;
	m_needSessionPassword = needPassword;
	emit sessionConfirmationNeeded(title, nsfm, autoJoin);
}

void LoginHandler::confirmJoinSelectedSession()
{
	if(m_needSessionPassword && !m_sessions->isModeratorMode()) {
		setState(WAIT_FOR_JOIN_PASSWORD);
		QString joinPasswordFromUrl = QUrlQuery{m_address}.queryItemValue(
			QStringLiteral("p"), QUrl::FullyDecoded);
		if(joinPasswordFromUrl.isEmpty()) {
			emit sessionPasswordNeeded();
		} else {
			m_joinPassword = joinPasswordFromUrl;
			sendJoinCommand();
		}

	} else {
		sendJoinCommand();
	}
}

void LoginHandler::sendJoinCommand()
{
	QJsonObject kwargs = makeClientInfo();
	if(!m_joinPassword.isEmpty()) {
		kwargs["password"] = m_joinPassword;
	}

	send("join", {m_selectedId}, kwargs);
	setState(EXPECT_LOGIN_OK);
}

void LoginHandler::reportSession(const QString &id, const QString &reason)
{
	send("report", {}, {{"session", id}, {"reason", reason}});
}

void LoginHandler::startTls()
{
	if(!m_server->loginStartTls(this)) {
		failLogin(tr("TLS is not supported via this kind of socket"));
	}
}

void LoginHandler::tlsError(const QList<QSslError> &errors)
{
#ifdef __EMSCRIPTEN__
	Q_UNUSED(errors);
	failLogin(tr("TLS is not supported via this kind of socket"));
#else
	QList<QSslError> ignore;
	QString errorstr;
	bool fail = false;

	// TODO this was optimized for self signed certificates back end Let's
	// Encrypt didn't exist. This should be fixed to better support actual CAs.
	bool isIp = QHostAddress().setAddress(m_address.host());
	if(looksLikeSelfSignedCertificate(m_server->hostCertificate(), errors)) {
		m_server->m_selfSignedCertificate = true;
	}
	qCDebug(lcDpLogin) << errors.size() << "SSL error(s), self-signed"
					   << m_server->m_selfSignedCertificate;

	for(const QSslError &e : errors) {
		if(e.error() == QSslError::SelfSignedCertificate) {
			// Self signed certificates are acceptable.
			qCInfo(lcDpLogin)
				<< "Ignoring self-signed certificate error:" << int(e.error())
				<< e.errorString();
			ignore << e;

		} else if(isIp && e.error() == QSslError::HostNameMismatch) {
			// Ignore CN mismatch when using an IP address rather than a
			// hostname
			ignore << e;
			qCInfo(lcDpLogin)
				<< "Ignoring error about hostname mismatch with IP address:"
				<< int(e.error()) << e.errorString();

		} else if(
			e.error() == QSslError::CertificateUntrusted &&
			m_server->m_selfSignedCertificate) {
			// "The root CA certificate is not trusted for this purpose" is an
			// error that spontaneously manifested in macOS and then way later
			// in Windows. We ignore it on self-signed certificates.
			ignore << e;
			qCInfo(lcDpLogin)
				<< "Ignoring untrusted error on self-signed certificate:"
				<< int(e.error()) << e.errorString();

		} else {
			fail = true;

			qCWarning(lcDpLogin)
				<< "SSL error:" << int(e.error()) << e.errorString();
			errorstr += e.errorString();
			errorstr += "\n";
		}
	}

	if(fail) {
		failLogin(errorstr);
	} else if(!m_server->loginIgnoreTlsErrors(ignore)) {
		failLogin(tr("Unable to set TLS error ignore state"));
	}
#endif
}

namespace {
void saveCert(const QFileInfo &file, const QSslCertificate &cert)
{
	QString filename = file.absoluteFilePath();

	QFile certOut(filename);

	if(certOut.open(DP_QT_WRITE_FLAGS))
		certOut.write(cert.toPem());
	else
		qCWarning(lcDpLogin) << "Couldn't open" << filename << "for writing!";
}
}

void LoginHandler::tlsStarted()
{
	const QSslCertificate cert = m_server->hostCertificate();
	const QString hostname = m_address.host();

	// Check if this is a trusted certificate
	m_certFile = getCertFile(TRUSTED_HOSTS, hostname);

	if(m_certFile.exists()) {
		QList<QSslCertificate> trustedcerts =
			QSslCertificate::fromPath(m_certFile.absoluteFilePath());

		if(trustedcerts.isEmpty() || trustedcerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));

		} else if(trustedcerts.at(0) != cert) {
			failLogin(tr("Pinned certificate has changed!"));

		} else {
			// Certificate matches explicitly trusted one, proceed with login
			m_server->m_securityLevel = Server::TRUSTED_HOST;
			continueTls();
		}

		return;
	}

	// Okay, not a trusted certificate, but check if we've seen it before
	m_certFile = getCertFile(KNOWN_HOSTS, hostname);
	if(m_certFile.exists()) {
		QList<QSslCertificate> knowncerts =
			QSslCertificate::fromPath(m_certFile.absoluteFilePath());

		if(knowncerts.isEmpty() || knowncerts.at(0).isNull()) {
			failLogin(tr("Invalid SSL certificate for host %1").arg(hostname));
			return;
		}

		if(knowncerts.at(0) != cert) {
			// Certificate mismatch. If this is a self-signed certificate, ask
			// if the user wants to accept the new one, since those shouldn't
			// change on its own. If it's a "real" one, it's just a renewal.
			m_server->m_securityLevel = Server::NEW_HOST;
			if(m_server->m_selfSignedCertificate) {
				emit certificateCheckNeeded(cert, knowncerts.at(0));
				return;
			} else {
				saveCert(m_certFile, cert);
			}

		} else {
			m_server->m_securityLevel = Server::KNOWN_HOST;
		}

	} else {
		// Host not encountered yet: rember the certificate for next time
		saveCert(m_certFile, cert);
		m_server->m_securityLevel = Server::NEW_HOST;
	}

	// Certificate is acceptable
	continueTls();
}

void LoginHandler::acceptServerCertificate()
{
	// User accepted the mismatching certificate: save it for the future
	saveCert(m_certFile, m_server->hostCertificate());
	continueTls();
}

void LoginHandler::continueTls()
{
	// STARTTLS is the very first command that must be sent, if sent at all
	sendClientInfo();
}

void LoginHandler::cancelLogin()
{
	setState(ABORT_LOGIN);
	m_server->loginFailure(tr("Cancelled"), "CANCELLED");
}

void LoginHandler::handleError(const QString &code, const QString &msg)
{
	qCWarning(lcDpLogin) << "Login error:" << code << msg;
	if(lcDpLogin().isDebugEnabled()) {
		qCDebug(
			lcDpLogin, "State %d, password state %d, login intent %d",
			int(m_state), int(m_passwordState), int(m_loginIntent));
	}

	QString error;
	if(code == QStringLiteral("notFound")) {
		error = tr("Session not found!");
	} else if(code == QStringLiteral("badPassword")) {
		if(m_passwordState == WAIT_FOR_LOGIN_PASSWORD &&
		   m_loginIntent != LoginMethod::Guest) {
			emit badLoginPassword(
				m_loginIntent, m_address.host(), m_address.userName());
			return; // Not a fatal error, let the user try again.
		} else if(m_state == EXPECT_LOGIN_OK) {
			setState(WAIT_FOR_JOIN_PASSWORD);
			emit badSessionPassword();
			return; // Not a fatal error, let the user try again.
		} else {
			error = msg;
		}
	} else if(code == QStringLiteral("badUsername")) {
		error = tr("Invalid username!");
	} else if(code == QStringLiteral("bannedName")) {
		error = tr("This username has been locked");
	} else if(code == QStringLiteral("nameInUse")) {
		error = tr("Username already taken!");
	} else if(code == QStringLiteral("closed")) {
		error = m_mode == Mode::Join ? tr("Session is closed!")
									 : tr("Server is full!");
	} else if(code == QStringLiteral("unauthorizedHost")) {
		error = tr("Hosting not authorized");
	} else if(code == QStringLiteral("banned")) {
		error = tr("You have been banned from this session!");
	} else if(code == QStringLiteral("idInUse")) {
		error = tr("Session alias is reserved!");
	} else if(code == QStringLiteral("protoverold")) {
		error = QStringLiteral("%1\n\n%2")
					.arg(
						msg, tr("This usually means that your Drawpile version "
								"is too old. Do you need to update?"));
	} else if(code == QStringLiteral("lookupFailed")) {
		error = tr("Session not found, it may have ended or its invite link "
				   "has changed");
	} else if(code == QStringLiteral("lookupRequired")) {
		error = tr(
			"This server only allows joining sessions through a direct link.");
	} else if(code == QStringLiteral("mismatchedHostName")) {
		error = tr("Invalid host name.");
	} else if(code == QStringLiteral("passwordedHost")) {
		error = tr("You're not allowed to host public sessions here, only "
				   "personal sessions are allowed. You can switch from public "
				   "to personal in the Session tab.");
	} else {
		error = msg;
	}

	failLogin(error, code);
}

void LoginHandler::failLogin(const QString &message, const QString &errorcode)
{
#ifdef __EMSCRIPTEN__
	cancelBrowserAuth();
#endif
	setState(ABORT_LOGIN);
	m_server->loginFailure(message, errorcode);
}

void LoginHandler::send(
	const QString &cmd, const QJsonArray &args, const QJsonObject &kwargs,
	bool containsAvatar)
{
	ServerCommand sc{cmd, args, kwargs};
	net::Message msg = sc.toMessage();
	if(msg.isNull() && containsAvatar) {
		qCWarning(
			lcDpLogin, "Removing avatar from server command and trying again");
		sc.kwargs.remove("avatar");
		msg = sc.toMessage();
	}

	if(msg.isNull()) {
		failLogin(tr("Client failed to serialize command"));
	} else {
		if(lcDpLogin().isDebugEnabled()) {
			QJsonArray safeArgs = sc.args;
			QJsonObject safeKwargs = sc.kwargs;
			if(sc.cmd == QStringLiteral("ident")) {
				if(safeArgs.size() >= 2) {
					safeArgs[1] =
						QStringLiteral("***account password redacted***");
				}
				if(safeKwargs.contains(QStringLiteral("extauth"))) {
					safeKwargs[QStringLiteral("extauth")] =
						QStringLiteral("***extauth token redacted***");
				}
			} else if(
				sc.cmd == QStringLiteral("host") ||
				sc.cmd == QStringLiteral("join")) {
				if(safeKwargs.contains(QStringLiteral("password"))) {
					safeKwargs[QStringLiteral("password")] =
						QStringLiteral("***session password redacted***");
				}
			}
			qCDebug(lcDpLogin)
				<< "login -->" << sc.cmd << safeArgs << safeKwargs;
		}
		m_server->sendMessage(msg);
	}
}

QString LoginHandler::takeAvatar()
{
	QString result;
	if(!m_avatar.isNull()) {
		if(m_supportsCustomAvatars) {
			QByteArray bytes;
			{
				QBuffer a;
				if(m_avatar.save(&a, "PNG")) {
					bytes = a.buffer();
				}
			}
			if(!bytes.isEmpty()) {
				result = QString::fromUtf8(bytes.toBase64());
			}
		}
		// Avatar only needs to be sent once.
		m_avatar = QPixmap();
	}
	return result;
}

LoginHandler::LoginMethod LoginHandler::parseLoginMethod(const QString &method)
{
	if(method == QStringLiteral("guest")) {
		return LoginMethod::Guest;
	} else if(method == QStringLiteral("auth")) {
		return LoginMethod::Auth;
	} else if(method == QStringLiteral("extauth")) {
		return LoginMethod::ExtAuth;
	} else {
		return LoginMethod::Unknown;
	}
}

QString LoginHandler::loginMethodToString(LoginMethod method)
{
	switch(method) {
	case LoginMethod::Unknown:
		return QString();
	case LoginMethod::Guest:
		return QStringLiteral("guest");
	case LoginMethod::Auth:
		return QStringLiteral("auth");
	case LoginMethod::ExtAuth:
		return QStringLiteral("extauth");
	}
	qCWarning(lcDpLogin, "Unhandled login method %d", int(method));
	return QString();
}

#ifndef __EMSCRIPTEN__
bool LoginHandler::looksLikeSelfSignedCertificate(
	const QSslCertificate &cert, const QList<QSslError> &errors)
{
	// The isSelfSigned member function only checks if the issuer and subject
	// are identical and logs an annoying warning to that effect in the process.
	// So prefer checking if we got a self-signed error instead to avoid that.
	for(const QSslError &e : errors) {
		if(e.error() == QSslError::SelfSignedCertificate) {
			return true;
		}
	}
	return cert.isSelfSigned();
}
#endif

QJsonObject LoginHandler::makeClientInfo()
{
	// Android reports "linux" as the kernel type, which is not helpful.
#if defined(Q_OS_ANDROID)
	QString os = QSysInfo::productType();
#else
	QString os = QSysInfo::kernelType();
#endif
	return QJsonObject{
		{"app_version", cmake_config::version()},
		{"protocol_version", DP_PROTOCOL_VERSION},
		{"qt_version", QString::number(QT_VERSION_MAJOR)},
		{"os", os},
		{"s", getSid()},
		{"m", QString::fromUtf8(QSysInfo::machineUniqueId().toBase64())},
		// Comma-separated list of client capabilities. KEEPALIVE indicates
		// support for DP_MSG_KEEP_ALIVE messages from the server.
		{"capabilities", QStringLiteral("KEEPALIVE")},
	};
}

QString LoginHandler::getSid()
{
	static QString key1{"5dd7038779f243b68001dab548ae59ab"};
	static QString key2{"56e06fe173a345dd983fe63671188093"};

	QSettings cfg1{
		QSettings::NativeFormat, QSettings::UserScope,
		QStringLiteral("drawpile"), QStringLiteral("sid")};
	QSettings cfg2{
		QSettings::IniFormat, QSettings::UserScope, QStringLiteral("drawpile"),
		QStringLiteral("system")};

	bool haveSid1 = cfg1.contains(key1);
	bool haveSid2 = cfg2.contains(key2);
	if(haveSid1 && haveSid2) {
		QString sid1 = cfg1.value(key1).toString();
		QString sid2 = cfg2.value(key2).toString();
		return sid1 == sid2 ? sid1 : generateTamperSid();
	} else if(haveSid1 && !haveSid2) {
		QString sid = cfg1.value(key1).toString();
		cfg2.setValue(key2, sid);
		return sid;
	} else if(!haveSid1 && haveSid2) {
		QString sid = cfg2.value(key2).toString();
		cfg1.setValue(key1, sid);
		return sid;
	} else {
		QString sid = generateSid();
		cfg1.setValue(key1, sid);
		cfg2.setValue(key2, sid);
		return sid;
	}
}

QString LoginHandler::generateTamperSid()
{
	QString sid = generateSid();
	int pos = QRandomGenerator::global()->bounded(sid.length() - 1);
	sid.replace(pos, 1, 'O');
	return sid;
}

QString LoginHandler::generateSid()
{
	return QUuid::createUuid().toString(QUuid::Id128);
}

}
