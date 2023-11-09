// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/loginhandler.h"
#include "cmake-config/config.h"
#include "libserver/client.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libserver/sessions.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/authtoken.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/validators.h"
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>

namespace server {

Sessions::~Sessions() {}

LoginHandler::LoginHandler(
	Client *client, Sessions *sessions, ServerConfig *config)
	: QObject(client)
	, m_client(client)
	, m_sessions(sessions)
	, m_config(config)
{
	connect(
		client, &Client::loginMessage, this, &LoginHandler::handleLoginMessage);
}

void LoginHandler::startLoginProcess()
{
	m_mandatoryLookup = m_config->getConfigBool(config::m_mandatoryLookup);
	m_state = m_mandatoryLookup ? State::WaitForLookup : State::WaitForIdent;

	QJsonArray flags;
	if(m_config->getConfigInt(config::SessionCountLimit) > 1) {
		flags << "MULTI";
	}
	if(m_config->getConfigBool(config::EnablePersistence)) {
		flags << "PERSIST";
	}
	if(m_client->hasSslSupport()) {
		flags << "TLS"
			  << "SECURE";
		m_state = State::WaitForSecure;
	}
	if(!m_config->getConfigBool(config::AllowGuests)) {
		flags << "NOGUEST";
	}
	if(m_config->internalConfig().reportUrl.isValid() &&
	   m_config->getConfigBool(config::AbuseReport)) {
		flags << "REPORT";
	}
	if(m_config->getConfigBool(config::AllowCustomAvatars)) {
		flags << "AVATAR";
	}
#ifdef HAVE_LIBSODIUM
	if(!m_config->internalConfig().cryptKey.isEmpty()) {
		flags << "CBANIMPEX";
	}
#endif
	flags << "MBANIMPEX" // Moderators can always export bans.
		  << "LOOKUP";	 // This server supports lookups.

	QJsonObject methods;
	if(m_config->getConfigBool(config::AllowGuests)) {
		QJsonArray guestActions = {QStringLiteral("join")};
		if(m_config->getConfigBool(config::AllowGuestHosts)) {
			guestActions.append(QStringLiteral("host"));
		}
		methods[QStringLiteral("guest")] = QJsonObject{{
			{QStringLiteral("actions"), guestActions},
		}};
	}

	if(m_config->hasAnyUserAccounts()) {
		// We could do a check here if any accounts actually exist that are
		// allowed to host, but that's kind of annoying to implement and pretty
		// unlikely to be useful in reality, more likely just confusing people
		// why they can't pick the server account option when hosting.
		methods[QStringLiteral("auth")] = QJsonObject{{
			{QStringLiteral("actions"),
			 QJsonArray{QStringLiteral("join"), QStringLiteral("host")}},
		}};
	}

	const QUrl extAuthUrl = m_config->internalConfig().extAuthUrl;
	if(extAuthUrl.isValid() && m_config->getConfigBool(config::UseExtAuth)) {
		QJsonArray extauthActions = {QStringLiteral("join")};
		if(m_config->getConfigBool(config::ExtAuthHost)) {
			extauthActions.append(QStringLiteral("host"));
		}
		methods[QStringLiteral("extauth")] = QJsonObject{
			{{QStringLiteral("actions"), extauthActions},
			 {QStringLiteral("url"), extAuthUrl.toString()}}};
	}

	// Start by telling who we are and what we can do
	send(net::ServerReply::makeLoginGreeting(
		QStringLiteral("Drawpile server %1").arg(cmake_config::version()),
		cmake_config::proto::server(), flags, methods,
		m_config->getConfigString(config::LoginInfoUrl),
		m_config->getConfigString(config::RuleText).trimmed()));

	// Client should disconnect upon receiving the above if the version number
	// does not match
}

void LoginHandler::announceServerInfo()
{
	const QJsonArray sessions = m_mandatoryLookup || !m_lookup.isEmpty()
									? QJsonArray()
									: m_sessions->sessionDescriptions();

	QString message = QStringLiteral("Welcome");
	QString title = m_config->getConfigString(config::ServerTitle);
	bool wholeMessageSent = send(
		title.isEmpty()
			? net::ServerReply::makeLoginSessions(message, sessions)
			: net::ServerReply::makeLoginWelcome(message, title, sessions));
	if(!wholeMessageSent) {
		// Reply was too long to fit in the message envelope!
		// Split the reply into separate announcements and send it in pieces
		if(!title.isEmpty()) {
			send(net::ServerReply::makeLoginTitle(message, title));
		}
		for(const QJsonValue &session : sessions) {
			send(net::ServerReply::makeLoginSessions(message, {session}));
		}
	}
}

void LoginHandler::announceSession(const QJsonObject &session)
{
	Q_ASSERT(session.contains("id"));
	if(m_state == State::WaitForLogin && !m_mandatoryLookup) {
		send(net::ServerReply::makeLoginSessions(
			QStringLiteral("New session"), {session}));
	}
}

void LoginHandler::announceSessionEnd(const QString &id)
{
	if(m_state == State::WaitForLogin && !m_mandatoryLookup) {
		send(net::ServerReply::makeLoginRemoveSessions(
			QStringLiteral("Session ended"), {id}));
	}
}

void LoginHandler::handleLoginMessage(const net::Message &msg)
{
	if(msg.type() != DP_MSG_SERVER_COMMAND) {
		m_client->log(
			Log()
				.about(Log::Level::Error, Log::Topic::RuleBreak)
				.message("Login handler was passed a non-login message"));
		return;
	}

	net::ServerCommand cmd = net::ServerCommand::fromMessage(msg);

	if(m_state == State::Ignore) {
		// Either the client is supposed to get disconnected or we're
		// intentionally leaving them hanging because they're banned.
	} else if(m_state == State::WaitForSecure) {
		// Secure mode: wait for STARTTLS before doing anything
		if(cmd.cmd == "startTls") {
			handleStarttls();
		} else {
			m_client->log(Log()
							  .about(Log::Level::Error, Log::Topic::RuleBreak)
							  .message("Client did not upgrade to TLS mode!"));
			sendError("tlsRequired", "TLS required");
		}

	} else if(m_state == State::WaitForLookup) {
		// Client must either look up a session to join or want to host.
		if(cmd.cmd == QStringLiteral("lookup")) {
			handleLookupMessage(cmd);
		} else {
			sendError(
				QStringLiteral("clientLookupUnsupported"),
				QStringLiteral("This server requires session lookups. You must "
							   "update Drawpile for this to work."));
		}

	} else if(m_state == State::WaitForIdent) {
		// Wait for user identification before moving on to session listing
		if(cmd.cmd == QStringLiteral("lookup")) {
			handleLookupMessage(cmd);
		} else if(cmd.cmd == "ident") {
			handleIdentMessage(cmd);
		} else {
			m_client->log(
				Log()
					.about(Log::Level::Error, Log::Topic::RuleBreak)
					.message(
						"Invalid login command (while waiting for ident): " +
						cmd.cmd));
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message");
		}
	} else {
		if(cmd.cmd == "host") {
			handleHostMessage(cmd);
		} else if(cmd.cmd == "join") {
			handleJoinMessage(cmd);
		} else if(cmd.cmd == "report") {
			handleAbuseReport(cmd);
		} else {
			m_client->log(Log()
							  .about(Log::Level::Error, Log::Topic::RuleBreak)
							  .message(
								  "Invalid login command (while waiting for "
								  "join/host): " +
								  cmd.cmd));
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message");
		}
	}
}

#if HAVE_LIBSODIUM
static QStringList jsonArrayToStringList(const QJsonArray &a)
{
	QStringList sl;
	for(const auto &v : a) {
		const auto s = v.toString();
		if(!s.isEmpty())
			sl << s;
	}
	return sl;
}
#endif

void LoginHandler::handleLookupMessage(const net::ServerCommand &cmd)
{
	if(!m_lookup.isNull()) {
		sendError(
			QStringLiteral("lookupAlreadyDone"),
			QStringLiteral("Session lookup already done"));
	}

	compat::sizetype argc = cmd.args.size();
	QString sessionIdOrAlias;
	net::Message msg;
	if(argc == 0) {
		sessionIdOrAlias = QStringLiteral("");
		msg = net::ServerReply::makeResultHostLookup(
			QStringLiteral("Host lookup OK!"));

	} else if(argc == 1) {
		sessionIdOrAlias = cmd.args[0].toString();
		if(sessionIdOrAlias.isEmpty()) {
			if(m_mandatoryLookup) {
				sendError(
					QStringLiteral("lookupRequired"),
					QStringLiteral("This server only allows joining sessions "
								   "through a direct link."));
				return;
			}
			sessionIdOrAlias = QStringLiteral("");
			msg = net::ServerReply::makeResultJoinLookup(
				QStringLiteral("Empty join lookup OK!"), QJsonObject());

		} else {
			Session *session =
				m_sessions->getSessionById(sessionIdOrAlias, false);
			if(!session) {
				sendError(
					"lookupFailed",
					QStringLiteral(
						"Session not found, it may have ended or its "
						"invite link has changed"));
				return;
			}
			msg = net::ServerReply::makeResultJoinLookup(
				QStringLiteral("Join lookup OK!"), session->getDescription());
		}

	} else {
		sendError(
			QStringLiteral("syntax"),
			QStringLiteral("Expected optional session id or alias"));
		return;
	}

	m_lookup = sessionIdOrAlias;
	m_state = State::WaitForIdent;
	send(msg);
}

void LoginHandler::handleIdentMessage(const net::ServerCommand &cmd)
{
	if(cmd.args.size() != 1 && cmd.args.size() != 2) {
		sendError("syntax", "Expected username and (optional) password");
		return;
	}

	const QString username = cmd.args[0].toString();
	const QString password =
		cmd.args.size() > 1 ? cmd.args[1].toString() : QString();

	if(!validateUsername(username)) {
		sendError("badUsername", "Invalid username");
		return;
	}

	IdentIntent intent =
		parseIdentIntent(cmd.kwargs[QStringLiteral("intent")].toString());
	if(intent == IdentIntent::Invalid) {
		sendError(
			QStringLiteral("identError"),
			QStringLiteral("Invalid ident intent"));
		return;
	}

	const RegisteredUser userAccount =
		m_config->getUserAccount(username, password);
	if(userAccount.status != RegisteredUser::NotFound) {
		if(cmd.kwargs.contains("extauth")) {
			// This should never happen. If it does, it means there's a bug in
			// the client or someone is probing for bugs in the server.
			sendError(
				"extAuthError",
				"Cannot use extauth with an internal user account!");
			return;
		} else if(!checkIdentIntent(intent, IdentIntent::Auth)) {
			return;
		}
	}

	if(cmd.kwargs.contains("avatar") &&
	   m_config->getConfigBool(config::AllowCustomAvatars)) {
		// TODO validate
		m_client->setAvatar(
			QByteArray::fromBase64(cmd.kwargs["avatar"].toString().toUtf8()));
	}

	switch(userAccount.status) {
	case RegisteredUser::NotFound: {
		// Account not found in internal user list. Allow guest login (if
		// enabled) or require external authentication
		const bool allowGuests = m_config->getConfigBool(config::AllowGuests);
		const bool useExtAuth =
			m_config->internalConfig().extAuthUrl.isValid() &&
			m_config->getConfigBool(config::UseExtAuth);

		if(useExtAuth) {
#ifdef HAVE_LIBSODIUM
			if(cmd.kwargs.contains("extauth")) {
				// An external authentication token was provided
				if(m_extauth_nonce == 0) {
					sendError("extAuthError", "Ext auth not requested!");
					return;
				}
				const AuthToken extAuthToken(
					cmd.kwargs["extauth"].toString().toUtf8());
				const QByteArray key = QByteArray::fromBase64(
					m_config->getConfigString(config::ExtAuthKey).toUtf8());
				if(!extAuthToken.checkSignature(key)) {
					sendError(
						"extAuthError", "Ext auth token signature mismatch!");
					return;
				}
				if(!extAuthToken.validatePayload(
					   m_config->getConfigString(config::ExtAuthGroup),
					   m_extauth_nonce)) {
					sendError("extAuthError", "Ext auth token is invalid!");
					return;
				}

				// Token is valid: log in as an authenticated user
				const QJsonObject ea = extAuthToken.payload();
				const QJsonValue uid = ea["uid"];

				bool extAuthMod = m_config->getConfigBool(config::ExtAuthMod);
				bool extAuthBanExempt =
					m_config->getConfigBool(config::ExtAuthBanExempt);
				QStringList flags =
					jsonArrayToStringList(ea["flags"].toArray());
				m_client->applyBanExemption(
					(extAuthMod && flags.contains("MOD")) ||
					(extAuthBanExempt && flags.contains("BANEXEMPT")));

				if(uid.isDouble() && !verifyUserId(uid.toDouble())) {
					return;
				}

				// We need some unique identifier. If the server didn't provide
				// one, the username is better than nothing.
				QString extAuthId =
					uid.isDouble() ? QString::number(qlonglong(uid.toDouble()))
								   : uid.toString();
				if(extAuthId.isEmpty())
					extAuthId = ea["username"].toString();

				// Prefix to identify this auth ID as an ext-auth ID
				extAuthId = m_config->internalConfig().extAuthUrl.host() + ":" +
							extAuthId;

				QByteArray avatar;
				if(m_config->getConfigBool(config::ExtAuthAvatars))
					avatar = extAuthToken.avatar();

				authLoginOk(
					ea["username"].toString(), extAuthId, flags, avatar,
					m_config->getConfigBool(config::ExtAuthMod),
					m_config->getConfigBool(config::ExtAuthHost),
					m_config->getConfigBool(config::EnableGhosts) &&
						m_config->getConfigBool(config::ExtAuthGhosts));

			} else if(allowGuests) {
				// No ext-auth token provided, but both guest logins and extauth
				// are enabled. We have to query if this account exists to check
				// which case we're dealing with.
				extAuthGuestLogin(username, intent);
			} else {
				// No ext-auth token provided, start external authentication.
				if(checkIdentIntent(intent, IdentIntent::ExtAuth)) {
					requestExtAuth();
				}
			}
#else
			// This should never be reached
			sendError(
				"extAuthError",
				"Server misconfiguration: ext-auth support not compiled in.");
#endif
			return;

		} else if(allowGuests) {
			// ExtAuth not enabled: permit guest login if enabled
			guestLogin(username, intent);
			return;
		}
		Q_FALLTHROUGH(); // fall through to badpass if guest logins are disabled
	}
	case RegisteredUser::BadPass:
		if(password.isEmpty()) {
			// No password: tell client that guest login is not possible (for
			// this username)
			m_state = State::WaitForIdent;
			send(net::ServerReply::makeResultPasswordNeeded(
				QStringLiteral("Password needed"),
				QStringLiteral("needPassword")));

		} else {
			sendError("badPassword", "Incorrect password", false);
		}
		return;

	case RegisteredUser::Banned:
		sendError("bannedName", "This username is banned");
		return;

	case RegisteredUser::Ok:
		// Yay, username and password were valid!
		m_client->applyBanExemption(
			userAccount.flags.contains("MOD") ||
			userAccount.flags.contains("BANEXEMPT"));
		authLoginOk(
			username, QStringLiteral("internal:%1").arg(userAccount.userId),
			userAccount.flags, QByteArray(), true, true,
			m_config->getConfigBool(config::EnableGhosts));
		break;
	}
}

void LoginHandler::authLoginOk(
	const QString &username, const QString &authId, const QStringList &flags,
	const QByteArray &avatar, bool allowMod, bool allowHost, bool allowGhost)
{
	Q_ASSERT(!authId.isEmpty());
	bool isMod = flags.contains(QStringLiteral("MOD")) && allowMod;
	bool wantGhost = flags.contains(QStringLiteral("GHOST"));
	bool isGhost = wantGhost && allowGhost;
	if(wantGhost) {
		if(!isGhost) {
			sendError(
				QStringLiteral("ghostsNotAllowed"),
				QStringLiteral("Ghost mode not allowed"));
			return;
		} else if(!isMod) {
			sendError(
				QStringLiteral("ghostWithoutMod"),
				QStringLiteral("Ghost mode is only available for moderators"));
			return;
		}
	}

	m_client->setUsername(username);
	m_client->setAuthId(authId);
	m_client->setAuthFlags(flags);

	m_client->setModerator(isMod, isGhost);
	if(!avatar.isEmpty())
		m_client->setAvatar(avatar);
	m_hostPrivilege = flags.contains("HOST") && allowHost;

	if(m_client->triggerBan(true)) {
		m_state = State::Ignore;
		return;
	} else {
		m_state = State::WaitForLogin;
	}

	send(net::ServerReply::makeResultLoginOk(
		QStringLiteral("Authenticated login OK!"), QStringLiteral("identOk"),
		QJsonArray::fromStringList(flags), m_client->username(), false));
	announceServerInfo();
}

/**
 * @brief Make a request to the ext-auth server and check if guest login is
 * possible for the given username
 *
 * If guest login is possible, do that.
 * Otherwise request external authentication.
 *
 * If the authserver cannot be reached, guest login is permitted (fallback mode)
 * or not.
 * @param username
 */
void LoginHandler::extAuthGuestLogin(
	const QString &username, IdentIntent intent)
{
	QNetworkRequest req(m_config->internalConfig().extAuthUrl);
	req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

	QJsonObject o;
	o["username"] = username;
	const QString authGroup = m_config->getConfigString(config::ExtAuthGroup);
	if(!authGroup.isEmpty())
		o["group"] = authGroup;

	m_client->log(Log()
					  .about(Log::Level::Info, Log::Topic::Status)
					  .message(QStringLiteral("Querying auth server for %1...")
								   .arg(username)));
	QNetworkReply *reply =
		networkaccess::getInstance()->post(req, QJsonDocument(o).toJson());
	connect(reply, &QNetworkReply::finished, this, [=]() {
		reply->deleteLater();

		if(m_state != State::WaitForIdent) {
			sendError(
				"extauth", "Received auth serveer reply in unexpected state");
			return;
		}

		bool fail = false;
		if(reply->error() != QNetworkReply::NoError) {
			fail = true;
			m_client->log(
				Log()
					.about(Log::Level::Warn, Log::Topic::Status)
					.message("Auth server error: " + reply->errorString()));
		}

		QJsonDocument doc;
		if(!fail) {
			QJsonParseError error;
			doc = QJsonDocument::fromJson(reply->readAll(), &error);
			if(error.error != QJsonParseError::NoError) {
				fail = true;
				m_client->log(Log()
								  .about(Log::Level::Warn, Log::Topic::Status)
								  .message(
									  "Auth server JSON parse error: " +
									  error.errorString()));
			}
		}

		if(fail) {
			if(m_config->getConfigBool(config::ExtAuthFallback)) {
				// Fall back to guest logins
				guestLogin(username, intent, true);
			} else {
				// If fallback mode is disabled, deny all non-internal logins
				sendError("noExtAuth", "Authentication server is unavailable!");
			}
			return;
		}

		const QJsonObject obj = doc.object();
		const QString status = obj["status"].toString();
		if(status == "auth") {
			if(checkIdentIntent(intent, IdentIntent::ExtAuth)) {
				requestExtAuth();
			}

		} else if(status == "guest") {
			guestLogin(username, intent);

		} else if(status == "outgroup") {
			sendError(
				"extauthOutgroup",
				"This username cannot log in to this server");

		} else {
			sendError("extauth", "Unexpected ext-auth response: " + status);
		}
	});
}

void LoginHandler::requestExtAuth()
{
#ifdef HAVE_LIBSODIUM
	m_extauth_nonce = AuthToken::generateNonce();
	send(net::ServerReply::makeResultExtAuthNeeded(
		QStringLiteral("External authentication needed"),
		QStringLiteral("needExtAuth"),
		m_config->internalConfig().extAuthUrl.toString(),
		QString::number(m_extauth_nonce, 16),
		m_config->getConfigString(config::ExtAuthGroup),
		m_client->avatar().isEmpty() &&
			m_config->getConfigBool(config::ExtAuthAvatars)));
#else
	qFatal("Bug: requestExtAuth() called, even though libsodium is not "
		   "compiled in!");
#endif
}

void LoginHandler::guestLogin(
	const QString &username, IdentIntent intent, bool extAuthFallback)
{
	if(!checkIdentIntent(intent, IdentIntent::Guest, extAuthFallback)) {
		return;
	}

	if(!m_config->getConfigBool(config::AllowGuests)) {
		sendError("noGuest", "Guest logins not allowed");
		return;
	}

	m_client->setUsername(username);

	if(m_client->triggerBan(true)) {
		m_state = State::Ignore;
		return;
	} else {
		m_state = State::WaitForLogin;
	}

	send(net::ServerReply::makeResultLoginOk(
		QStringLiteral("Guest login OK!"), QStringLiteral("identOk"), {},
		m_client->username(), true));
	announceServerInfo();
}

static QJsonArray sessionFlags(const Session *session)
{
	QJsonArray flags;
	// Note: this is "NOAUTORESET" for backward compatibility. In 3.0, we should
	// change it to "AUTORESET"
	if(!session->supportsAutoReset())
		flags << "NOAUTORESET";
	// TODO for version 3.0: PERSIST should be a session specific flag

	return flags;
}

void LoginHandler::handleHostMessage(const net::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());
	// Basic validation
	if(!m_lookup.isEmpty()) {
		sendError(
			QStringLiteral("lookupHostMismatch"),
			QStringLiteral("Cannot host when a joining intent was given"));
		return;
	}

	if(!m_config->getConfigBool(config::AllowGuestHosts) && !m_hostPrivilege) {
		sendError("unauthorizedHost", "Hosting not authorized");
		return;
	}

	if(m_client->isGhost()) {
		sendError("ghostHost", "Hosting not available in ghost mode");
		return;
	}

	protocol::ProtocolVersion protocolVersion =
		protocol::ProtocolVersion::fromString(
			cmd.kwargs.value("protocol").toString());

	if(!protocolVersion.isValid()) {
		sendError("syntax", "Unparseable protocol version");
		return;
	}

	// Moderators can host under any protocol version, but everyone else has to
	// meet the minimum version, if one is configured.
	if(!m_client->isModerator()) {
		QString minimumProtocolVersionString =
			m_config->getConfigString(config::MinimumProtocolVersion);
		if(!minimumProtocolVersionString.isEmpty()) {
			protocol::ProtocolVersion minimumProtocolVersion =
				protocol::ProtocolVersion::fromString(
					minimumProtocolVersionString);
			if(minimumProtocolVersion.isValid()) {
				if(protocolVersion.ns() != minimumProtocolVersion.ns()) {
					sendError(
						QStringLiteral("protoverns"),
						QStringLiteral("Mismatched protocol namespace, minimum "
									   "protocol version is %1")
							.arg(minimumProtocolVersionString));
					return;
				} else if(!protocolVersion.isGreaterOrEqual(
							  minimumProtocolVersion)) {
					sendError(
						QStringLiteral("protoverold"),
						QStringLiteral(
							"Outdated client, minimum protocol version is %1")
							.arg(minimumProtocolVersionString));
					return;
				}
			} else {
				qWarning(
					"Invalid minimum protocol version '%s' configured",
					qUtf8Printable(minimumProtocolVersionString));
			}
		}
	}

	if(!verifySystemId(cmd, protocolVersion)) {
		return;
	}

	int userId = cmd.kwargs.value("user_id").toInt();

	if(userId < 1 || userId > 254) {
		sendError("syntax", "Invalid user ID (must be in range 1-254)");
		return;
	}

	m_client->setId(userId);

	QString sessionAlias = cmd.kwargs.value("alias").toString();
	if(!sessionAlias.isEmpty()) {
		if(!validateSessionIdAlias(sessionAlias)) {
			sendError("badAlias", "Invalid session alias");
			return;
		}
	}

	if(m_client->triggerBan(false)) {
		m_state = State::Ignore;
		return;
	}

	// Create a new session
	Session *session;
	QString sessionErrorCode;
	std::tie(session, sessionErrorCode) = m_sessions->createSession(
		Ulid::make().toString(), sessionAlias, protocolVersion,
		m_client->username());

	if(!session) {
		QString msg;
		if(sessionErrorCode == "idInUse")
			msg = "An internal server error occurred.";
		else if(sessionErrorCode == "badProtocol")
			msg = "This server does not support this protocol version.";
		else if(sessionErrorCode == "closed")
			msg = "This server is full.";
		else
			msg = sessionErrorCode;

		sendError(sessionErrorCode, msg);
		return;
	}

	if(cmd.kwargs["password"].isString())
		session->history()->setPassword(cmd.kwargs["password"].toString());

	// Mark login phase as complete.
	// No more login messages will be sent to this user.
	send(net::ServerReply::makeResultJoinHost(
		QStringLiteral("Starting new session!"), QStringLiteral("host"),
		{{QStringLiteral("id"),
		  sessionAlias.isEmpty() ? session->id() : sessionAlias},
		 {QStringLiteral("user"), userId},
		 {QStringLiteral("flags"), sessionFlags(session)},
		 {QStringLiteral("authId"), m_client->authId()}}));

	m_complete = true;
	session->joinUser(m_client, true);
	logClientInfo(cmd);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const net::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());
	if(cmd.args.size() != 1) {
		sendError("syntax", "Expected session ID");
		return;
	}

	if(m_mandatoryLookup && m_lookup.isEmpty()) {
		sendError(
			QStringLiteral("lookupJoinMismatch"),
			QStringLiteral("Cannot join when a hosting intent was given"));
		return;
	}

	QString sessionId = cmd.args.at(0).toString();
	if(!m_lookup.isEmpty() && sessionId != m_lookup) {
		sendError(
			QStringLiteral("lookupMismatch"),
			QStringLiteral("Cannot look up one session and then join another"));
		return;
	}

	Session *session = m_sessions->getSessionById(sessionId, true);
	if(!session) {
		sendError("notFound", "Session not found!");
		return;
	}

	if(!verifySystemId(cmd, session->history()->protocolVersion())) {
		return;
	}

	if(!m_client->isModerator()) {
		// Non-moderators have to obey access restrictions
		int banId = session->history()->banlist().isBanned(
			m_client->username(), m_client->peerAddress(), m_client->authId(),
			m_client->sid());
		if(banId != 0) {
			session->log(
				Log()
					.about(Log::Level::Info, Log::Topic::Ban)
					.user(
						m_client->id(), m_client->peerAddress(),
						m_client->username())
					.message(
						QStringLiteral("Join prevented by ban %1").arg(banId)));
			sendError(
				"banned", QStringLiteral(
							  "You have been banned from this session (ban %1)")
							  .arg(banId));
			return;
		}
		if(session->isClosed()) {
			sendError("closed", "This session is closed");
			return;
		}
		if(session->history()->hasFlag(SessionHistory::AuthOnly) &&
		   !m_client->isAuthenticated()) {
			sendError("authOnly", "This session does not allow guest logins");
			return;
		}

		if(!session->history()->checkPassword(
			   cmd.kwargs.value("password").toString())) {
			sendError("badPassword", "Incorrect password", false);
			return;
		}
	}

	if(session->getClientByUsername(m_client->username())) {
#ifdef NDEBUG
		sendError("nameInuse", "This username is already in use");
		return;
#else
		// Allow identical usernames in debug builds, so I don't have to keep
		// changing the username when testing. There is no technical requirement
		// for unique usernames; the limitation is solely for the benefit of the
		// human users.
		m_client->log(
			Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(
					"Username clash ignored because this is a debug build."));
#endif
	}

	if(m_client->triggerBan(false)) {
		m_state = State::Ignore;
		return;
	}

	// Ok, join the session
	session->assignId(m_client);
	send(net::ServerReply::makeResultJoinHost(
		QStringLiteral("Joining a session!"), QStringLiteral("join"),
		{{QStringLiteral("id"), session->aliasOrId()},
		 {QStringLiteral("user"), m_client->id()},
		 {QStringLiteral("flags"), sessionFlags(session)},
		 {QStringLiteral("authId"), m_client->authId()}}));

	m_complete = true;
	session->joinUser(m_client, false);
	logClientInfo(cmd);

	deleteLater();
}

void LoginHandler::logClientInfo(const net::ServerCommand &cmd)
{
	QJsonObject info;
	QString keys[] = {
		QStringLiteral("app_version"), QStringLiteral("protocol_version"),
		QStringLiteral("qt_version"),  QStringLiteral("os"),
		QStringLiteral("s"),
	};
	for(const QString &key : keys) {
		if(cmd.kwargs.contains(key)) {
			QJsonValue value = cmd.kwargs[key];
			if(value.isString()) {
				QString s = value.toString().trimmed();
				if(!s.isEmpty()) {
					s.truncate(64);
					info[key] = s;
				}
			}
		}
	}


	info[QStringLiteral("address")] = m_client->peerAddress().toString();
	if(m_client->isAuthenticated()) {
		info[QStringLiteral("auth_id")] = m_client->authId();
	}
	m_client->log(
		Log()
			.about(Log::Level::Info, Log::Topic::ClientInfo)
			.message(QJsonDocument(info).toJson(QJsonDocument::Compact)));
}

void LoginHandler::handleAbuseReport(const net::ServerCommand &cmd)
{
	// We don't tell users that they're banned until they actually attempt to
	// join or host a session, so they don't get to make reports before that.
	if(!m_client->isBanInProgress()) {
		Session *s =
			m_sessions->getSessionById(cmd.kwargs["session"].toString(), false);
		if(s) {
			s->sendAbuseReport(m_client, 0, cmd.kwargs["reason"].toString());
		}
	}
}

void LoginHandler::handleStarttls()
{
	if(!m_client->hasSslSupport()) {
		// Note. Well behaved clients shouldn't send STARTTLS if TLS was not
		// listed in server features.
		sendError("noTls", "TLS not supported");
		return;
	}

	if(m_client->isSecure()) {
		sendError(
			"alreadySecure",
			"Connection already secured"); // shouldn't happen normally
		return;
	}

	send(net::ServerReply::makeResultStartTls(
		QStringLiteral("Start TLS now!"), true));

	m_client->startTls();
	m_state = m_mandatoryLookup ? State::WaitForLookup : State::WaitForIdent;
}

bool LoginHandler::send(const net::Message &msg)
{
	if(!m_complete) {
		if(msg.isNull()) {
			qWarning("Login message is null (input oversized?)");
			return false;
		}
		m_client->sendDirectMessage(msg);
	}
	return true;
}

void LoginHandler::sendError(
	const QString &code, const QString &message, bool disconnect)
{
	send(net::ServerReply::makeError(message, code));
	if(disconnect) {
		m_state = State::Ignore;
		m_client->disconnectClient(
			Client::DisconnectionReason::Error, "Login error");
	}
}

LoginHandler::IdentIntent LoginHandler::parseIdentIntent(const QString &s)
{
	if(s.isEmpty()) {
		return IdentIntent::Unknown;
	} else if(s == QStringLiteral("guest")) {
		return IdentIntent::Guest;
	} else if(s == QStringLiteral("auth")) {
		return IdentIntent::Auth;
	} else if(s == QStringLiteral("extauth")) {
		return IdentIntent::ExtAuth;
	} else {
		return IdentIntent::Invalid;
	}
}

QString LoginHandler::identIntentToString(IdentIntent intent)
{
	switch(intent) {
	case IdentIntent::Invalid:
		return QStringLiteral("invalid");
	case IdentIntent::Unknown:
		return QStringLiteral("unknown");
	case IdentIntent::Guest:
		return QStringLiteral("guest");
	case IdentIntent::Auth:
		return QStringLiteral("auth");
	case IdentIntent::ExtAuth:
		return QStringLiteral("extauth");
	}
	qWarning("Unhandled ident intent %d", int(intent));
	return QStringLiteral("unhandled");
}

bool LoginHandler::checkIdentIntent(
	IdentIntent intent, IdentIntent actual, bool extAuthFallback)
{
	Q_ASSERT(!extAuthFallback || actual == IdentIntent::Guest);
	if(intent == IdentIntent::Unknown || intent == actual) {
		return true;
	} else {
		send(net::ServerReply::makeResultIdentIntentMismatch(
			QStringLiteral("Intent mismatch"), identIntentToString(intent),
			identIntentToString(actual), extAuthFallback));
		return false;
	}
}

bool LoginHandler::verifySystemId(
	const net::ServerCommand &cmd, const protocol::ProtocolVersion &protover)
{
	QString sid = cmd.kwargs[QStringLiteral("s")].toString();
	m_client->setSid(sid);
	if(sid.isEmpty()) {
		if(protover.shouldHaveSystemId()) {
			m_client->log(Log()
							  .about(Log::Level::Error, Log::Topic::RuleBreak)
							  .message(QStringLiteral("Missing required sid")));
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message");
			return false;
		}
	} else if(!isValidSid(sid)) {
		m_client->log(Log()
						  .about(Log::Level::Error, Log::Topic::RuleBreak)
						  .message(QStringLiteral("Invalid sid %1").arg(sid)));
		m_client->disconnectClient(
			Client::DisconnectionReason::Error, "invalid message");
		return false;
	} else if(!m_client->isBanInProgress()) {
		BanResult ban = m_config->isSystemBanned(sid);
		m_client->applyBan(ban);
	}
	return true;
}

bool LoginHandler::isValidSid(const QString &sid)
{
	static QRegularExpression sidRe(QStringLiteral("\\A[0-9a-fA-F]{32}\\z"));
	return sidRe.match(sid).hasMatch();
}

bool LoginHandler::verifyUserId(long long userId)
{
	if(!m_client->isBanInProgress()) {
		BanResult ban = m_config->isUserBanned(userId);
		m_client->applyBan(ban);
	}
	return true;
}

}
