// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/net/login.h"
#include "cmake-config/config.h"
#include "libclient/net/loginsessions.h"
#include "libclient/net/tcpserver.h"
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
#include <utility>

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
	, m_state(EXPECT_HELLO)
	, m_passwordState(WAIT_FOR_LOGIN_PASSWORD)
	, m_multisession(false)
	, m_canPersist(false)
	, m_canReport(false)
	, m_needUserPassword(false)
	, m_supportsCustomAvatars(false)
	, m_supportsCryptBanImpEx(false)
	, m_supportsModBanImpEx(false)
	, m_supportsLookup(false)
	, m_supportsExtAuthAvatars(false)
	, m_compatibilityMode(false)
	, m_isGuest(true)
{
	m_sessions = new LoginSessionModel(this);

	// Automatically join a session if the ID is included in the URL
	QString path = m_address.path();
	if(path.length() > 1) {
		QRegularExpression idre("\\A/([a-zA-Z0-9:-]{1,64})/?\\z");
		auto m = idre.match(path);
		if(m.hasMatch())
			m_autoJoinId = m.captured(1);
	}
}

void LoginHandler::serverDisconnected() {}

bool LoginHandler::receiveMessage(const ServerReply &msg)
{
	if(lcDpLogin().isDebugEnabled()) {
		qCDebug(lcDpLogin) << "login <--" << msg.reply;
	}

	// Overall, the login process is:
	// 1. wait for server greeting
	// 2. Upgrade to secure connection (if available)
	// 3. Lookup session (if joining directly)
	// 4. Authenticate user (or do guest login)
	// 5. wait for session list
	// 6. wait for user to finish typing join password if needed
	// 7. send host/join command
	// 8. wait for OK

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
	m_canPersist = false;
	m_canReport = false;

	bool startTls = false;

	for(const QJsonValue &flag : flags) {
		if(flag == "MULTI") {
			m_multisession = true;
		} else if(flag == "TLS") {
			startTls = true;
		} else if(flag == "SECURE") {
			// Changed in 2.1.9 (although in practice we've always done this):
			// this flag is implied by TLS
		} else if(flag == "PERSIST") {
			m_canPersist = true;
		} else if(flag == "NOGUEST") {
			m_mustAuth = true;
		} else if(flag == "REPORT") {
			m_canReport = true;
		} else if(flag == "AVATAR") {
			m_supportsCustomAvatars = true;
		} else if(flag == "CBANIMPEX") {
			m_supportsCryptBanImpEx = true;
		} else if(flag == "MBANIMPEX") {
			m_supportsModBanImpEx = true;
		} else if(flag == "LOOKUP") {
			m_supportsLookup = true;
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
		m_state = EXPECT_STARTTLS;
		send("startTls");

	} else {
		// If this is a trusted host, it should always be in secure mode
		if(getCertFile(TRUSTED_HOSTS, m_address.host()).exists()) {
			failLogin(tr("Secure mode not enabled on a trusted host!"));
			return;
		}

		lookUpSession();
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

void LoginHandler::lookUpSession()
{
	if(m_supportsLookup) {
		m_state = EXPECT_LOOKUP_OK;
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
		m_state = WAIT_FOR_LOGIN_PASSWORD;

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

	m_state = EXPECT_IDENTIFIED;
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
			m_state = EXPECT_IDENTIFIED;
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
					   (!extAuthFallback || intent == LoginMethod::ExtAuth);
		if(isValid) {
			emit loginMethodMismatch(intent, method, extAuthFallback);
		} else {
			failLogin(tr("Invalid ident intent response."));
		}
		return;
	}

	if(state == QStringLiteral("needPassword")) {
		// Looks like guest logins are not possible
		m_needUserPassword = true;
		prepareToSendIdentity();
		return;
	}

	if(state == QStringLiteral("needExtAuth")) {
		// External authentication needed for this username
		m_state = WAIT_FOR_EXTAUTH;
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
		}

		m_extAuthGroup = msg.reply["group"].toString();
		m_extAuthNonce = msg.reply["nonce"].toString();

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

	emit loginOk(m_loginIntent, m_address.host(), m_address.userName());

	m_isGuest = msg.reply["guest"].toBool();
	for(const QJsonValue f : msg.reply["flags"].toArray())
		m_userFlags << f.toString().toUpper();

	m_sessions->setModeratorMode(m_userFlags.contains("MOD"));

	if(m_mode != Mode::Join) {
		m_state = EXPECT_SESSIONLIST_TO_HOST;

	} else {
		// Show session selector if in multisession mode
		// In single-session mode we can just automatically join
		// the first session we see.
		if(m_multisession)
			emit sessionChoiceNeeded(m_sessions);

		m_state = EXPECT_SESSIONLIST_TO_JOIN;
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
	QJsonObject kwargs = makeClientInfoKwargs();

	if(!m_sessionAlias.isEmpty())
		kwargs["alias"] = m_sessionAlias;

	kwargs["protocol"] = protocol::ProtocolVersion::current().asString();
	kwargs["user_id"] = m_userid;
	if(!m_sessionPassword.isEmpty()) {
		kwargs["password"] = m_sessionPassword;
		m_joinPassword = m_sessionPassword;
	}

	send("host", {}, kwargs);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::expectSessionDescriptionJoin(const ServerReply &msg)
{
	Q_ASSERT(m_mode != Mode::HostRemote);

	if(msg.reply.contains("title")) {
		emit serverTitleChanged(msg.reply["title"].toString());
	}

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
						session.compatibilityMode, session.title, session.nsfm,
						true);
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
					session.id, session.needPassword, session.compatibilityMode,
					session.title, session.nsfm, true);
			}
		}
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
					.arg(session.incompatibleSeries));
		}
		return false;
	}

	return true;
}

LoginSession LoginHandler::updateSession(const QJsonObject &js)
{
	protocol::ProtocolVersion protoVer =
		protocol::ProtocolVersion::fromString(js["protocol"].toString());

	QString incompatibleSeries;
	if(!protoVer.isCompatible()) {
		if(protoVer.isFuture()) {
			incompatibleSeries = tr("New version");
		} else {
			incompatibleSeries = protoVer.versionName();
		}

		if(incompatibleSeries.isEmpty()) {
			incompatibleSeries = tr("Unknown version");
		}
	}

	LoginSession session{
		js["id"].toString(),
		js["alias"].toString(),
		js["title"].toString(),
		js["founder"].toString(),
		incompatibleSeries,
		protoVer.isPastCompatible(),
		js["userCount"].toInt(),
		js["hasPassword"].toBool(),
		js["persistent"].toBool(),
		js["closed"].toBool(),
		js["authOnly"].toBool() && m_isGuest,
		js["nsfm"].toBool()};
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
		m_address.setPath("/" + msg.reply["join"].toObject()["id"].toString());

		QJsonValue join = msg.reply["join"];
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

			if(!m_title.isEmpty())
				kwargs["title"] = m_title;

			if(m_nsfm)
				kwargs["nsfm"] = true;

			send("sessionconf", {}, kwargs);

			if(!m_announceUrl.isEmpty())
				m_server->sendMessage(
					ServerCommand::makeAnnounce(m_announceUrl, false));

			// Upload initial session content
			if(m_mode == Mode::HostRemote) {
				m_server->sendMessages(
					m_initialState.count(), m_initialState.constData());
				send("init-complete");
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
		m_state = WAIT_FOR_JOIN_PASSWORD;
		m_passwordState = WAIT_FOR_JOIN_PASSWORD;
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
	QJsonObject kwargs = makeClientInfoKwargs();
	if(!m_joinPassword.isEmpty()) {
		kwargs["password"] = m_joinPassword;
	}

	send("join", {m_selectedId}, kwargs);
	m_state = EXPECT_LOGIN_OK;
}

void LoginHandler::reportSession(const QString &id, const QString &reason)
{
	send("report", {}, {{"session", id}, {"reason", reason}});
}

void LoginHandler::startTls()
{
	connect(
		m_server->m_socket, &QSslSocket::encrypted, this,
		&LoginHandler::tlsStarted);
	connect(
		m_server->m_socket,
		QOverload<const QList<QSslError> &>::of(&QSslSocket::sslErrors), this,
		&LoginHandler::tlsError);

	m_server->m_socket->startClientEncryption();
}

void LoginHandler::tlsError(const QList<QSslError> &errors)
{
	QList<QSslError> ignore;
	QString errorstr;
	bool fail = false;

	// TODO this was optimized for self signed certificates back end Let's
	// Encrypt didn't exist. This should be fixed to better support actual CAs.
	bool isIp = QHostAddress().setAddress(m_address.host());
	bool isSelfSigned = m_server->hostCertificate().isSelfSigned();
	qCDebug(lcDpLogin) << errors.size() << "SSL error(s), self-signed"
					   << isSelfSigned;

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
			e.error() == QSslError::CertificateUntrusted && isSelfSigned) {
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

	if(fail)
		failLogin(errorstr);
	else
		m_server->m_socket->ignoreSslErrors(ignore);
}

namespace {
void saveCert(const QFileInfo &file, const QSslCertificate &cert)
{
	QString filename = file.absoluteFilePath();

	QFile certOut(filename);

	if(certOut.open(QFile::WriteOnly))
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
			failLogin(tr("Certificate of a trusted server has changed!"));

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
			// Certificate mismatch!
			emit certificateCheckNeeded(cert, knowncerts.at(0));
			m_server->m_securityLevel = TcpServer::NEW_HOST;
			return;

		} else {
			m_server->m_securityLevel = TcpServer::KNOWN_HOST;
		}

	} else {
		// Host not encountered yet: rember the certificate for next time
		saveCert(m_certFile, cert);
		m_server->m_securityLevel = TcpServer::NEW_HOST;
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
	lookUpSession();
}

void LoginHandler::cancelLogin()
{
	m_state = ABORT_LOGIN;
	m_server->loginFailure(tr("Cancelled"), "CANCELLED");
}

void LoginHandler::handleError(const QString &code, const QString &msg)
{
	qCWarning(lcDpLogin) << "Login error:" << code << msg;

	QString error;
	if(code == "notFound")
		error = tr("Session not found!");
	else if(code == "badPassword") {
		if(m_passwordState == WAIT_FOR_LOGIN_PASSWORD &&
		   m_loginIntent != LoginMethod::Guest) {
			emit badLoginPassword(
				m_loginIntent, m_address.host(), m_address.userName());
			return; // Not a fatal error, let the user try again.
		} else if(m_state == EXPECT_LOGIN_OK) {
			m_state = WAIT_FOR_JOIN_PASSWORD;
			emit badSessionPassword();
			return; // Not a fatal error, let the user try again.
		} else {
			error = msg;
		}
	} else if(code == "badUsername")
		error = tr("Invalid username!");
	else if(code == "bannedName")
		error = tr("This username has been locked");
	else if(code == "nameInUse")
		error = tr("Username already taken!");
	else if(code == "closed")
		error = m_mode == Mode::Join ? tr("Session is closed!")
									 : tr("Server is full!");
	else if(code == "unauthorizedHost")
		error = tr("Hosting not authorized");
	else if(code == "banned")
		error = tr("You have been banned from this session!");
	else if(code == "idInUse")
		error = tr("Session alias is reserved!");
	else if(code == "protoverold")
		error = QStringLiteral("%1\n\n%2")
					.arg(
						msg, tr("This usually means that your Drawpile version "
								"is too old. Do you need to update?"));
	else if(code == "lookupFailed")
		error = tr("Session not found, it may have ended or its invite link "
				   "has changed");
	else if(code == "lookupRequired")
		error = tr(
			"This server only allows joining sessions through a direct link.");
	else
		error = msg;

	failLogin(error, code);
}

void LoginHandler::failLogin(const QString &message, const QString &errorcode)
{
	m_state = ABORT_LOGIN;
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

bool LoginHandler::hasUserFlag(const QString &flag) const
{
	return m_userFlags.contains(flag.toUpper());
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

QJsonObject LoginHandler::makeClientInfoKwargs()
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
