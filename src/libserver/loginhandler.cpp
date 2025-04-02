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
#include <utility>

namespace server {

namespace {
static QString clientInfoKeys[] = {
	QStringLiteral("app_version"), QStringLiteral("protocol_version"),
	QStringLiteral("qt_version"),  QStringLiteral("os"),
	QStringLiteral("s"),		   QStringLiteral("m"),
};
}

Sessions::~Sessions() {}

// Logs client info on destruction, to make sure we got it somewhere.
class LoginHandler::ClientInfoLogGuard {
public:
	ClientInfoLogGuard(LoginHandler *loginHandler, const QJsonObject &info)
		: m_loginHandler(loginHandler)
		, m_info(info)
	{
	}

	~ClientInfoLogGuard() { m_loginHandler->logClientInfo(m_info); }

	const QJsonObject &info() const { return m_info; }

	const QString appVersion() const
	{
		return m_info.value(QStringLiteral("app_version")).toString();
	}

	const QString sid() const
	{
		return m_info.value(QStringLiteral("s")).toString();
	}

private:
	LoginHandler *m_loginHandler;
	QJsonObject m_info;
};

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
	m_requireMatchingHost =
		m_config->getConfigBool(config::RequireMatchingHost);
	m_mandatoryLookup = m_config->getConfigBool(config::MandatoryLookup);
	m_minimumProtocolVersionString =
		m_config->getConfigString(config::MinimumProtocolVersion);
	if(!m_minimumProtocolVersionString.isEmpty()) {
		m_minimumProtocolVersion = protocol::ProtocolVersion::fromString(
			m_minimumProtocolVersionString);
		if(!m_minimumProtocolVersion.isValid()) {
			qWarning(
				"Invalid minimum protocol version '%s' configured",
				qUtf8Printable(m_minimumProtocolVersionString));
		}
	}

	QJsonArray flags;
	if(m_config->getConfigInt(config::SessionCountLimit) > 1) {
		flags << QStringLiteral("MULTI");
	}
	if(m_config->getConfigBool(config::EnablePersistence)) {
		flags << QStringLiteral("PERSIST");
	}
	if(m_client->hasSslSupport()) {
		flags << QStringLiteral("TLS") << QStringLiteral("SECURE");
		m_state = State::WaitForSecure;
	} else {
		m_state = m_requireMatchingHost ? State::WaitForClientInfo
				  : needsLookup()		? State::WaitForLookup
										: State::WaitForIdent;
	}
	bool browser = m_client->isBrowser();
	bool allowGuests =
		m_config->getConfigBool(config::AllowGuests) &&
		(!browser || m_config->getConfigBool(config::AllowGuestWeb));
	if(!allowGuests) {
		flags << QStringLiteral("NOGUEST");
	}
	if(m_config->internalConfig().reportUrl.isValid() &&
	   m_config->getConfigBool(config::AbuseReport)) {
		flags << QStringLiteral("REPORT");
	}
	if(m_config->getConfigBool(config::AllowCustomAvatars)) {
		flags << QStringLiteral("AVATAR");
	}
#ifdef HAVE_LIBSODIUM
	if(!m_config->internalConfig().cryptKey.isEmpty()) {
		flags << QStringLiteral("CBANIMPEX");
	}
#endif
	flags << QStringLiteral("MBANIMPEX") // Moderators can always export bans.
		  << QStringLiteral("LOOKUP")	 // This server supports lookups.
		  << QStringLiteral("CINFO");	 // Supports client info messages.

	QJsonObject methods;
	bool allowGuestHosts =
		m_config->getConfigBool(config::AllowGuestHosts) &&
		(!browser || m_config->getConfigBool(config::AllowGuestWebHosts));
	HostPrivilege guestHost = HostPrivilege::None;
	if(allowGuests) {
		QJsonArray guestActions = {QStringLiteral("join")};
		if(allowGuestHosts) {
			if(browser) {
				if(!m_config->getConfigBool(config::AllowGuestWebSession) &&
				   m_config->getConfigBool(
					   config::PasswordDependentWebSession)) {
					guestHost = HostPrivilege::Passworded;
				} else {
					guestHost = HostPrivilege::Any;
				}
			} else {
				guestHost = HostPrivilege::Any;
			}
		}
		if(guestHost != HostPrivilege::None) {
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
		switch(guestHost) {
		case HostPrivilege::Any:
		case HostPrivilege::Passworded:
			extauthActions.append(QStringLiteral("host"));
			break;
		default:
			if(allowGuestHosts ||
			   (m_config->getConfigBool(config::ExtAuthHost) &&
				(!browser ||
				 m_config->getConfigBool(config::ExtAuthWebHost)))) {
				extauthActions.append(QStringLiteral("host"));
			}
			break;
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
		m_config->getConfigString(config::RuleText).trimmed(),
		m_config->preferWebSockets()));

	// Client should disconnect upon receiving the above if the version number
	// does not match
}

void LoginHandler::announceServerInfo()
{
	const QJsonArray sessions =
		m_mandatoryLookup || !m_lookup.isEmpty()
			? QJsonArray()
			: m_sessions->sessionDescriptions(m_client->isModerator());
	for(const QJsonValue &session : sessions) {
		m_listedSessionIds.insert(session[QStringLiteral("id")].toString());
	}

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
		QString id = session.value(QStringLiteral("id")).toString();
		if(!m_client->isModerator() && session.value("unlisted").toBool()) {
			announceSessionEnd(id);
		} else {
			m_listedSessionIds.insert(id);
			send(net::ServerReply::makeLoginSessions(
				QStringLiteral("New session"), {session}));
		}
	}
}

void LoginHandler::announceSessionEnd(const QString &id)
{
	if(m_state == State::WaitForLogin && !m_mandatoryLookup &&
	   m_listedSessionIds.remove(id)) {
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

	} else if(m_state == State::WaitForClientInfo) {
		if(cmd.cmd == QStringLiteral("cinfo")) {
			handleClientInfoMessage(cmd);
		} else {
			sendError(
				QStringLiteral("clientInfoUnsupported"),
				QStringLiteral(
					"Your version of Drawpile is too old. To update, go to "
					"drawpile.net and download a newer version."));
		}

	} else if(m_state == State::WaitForLookup) {
		if(cmd.cmd == QStringLiteral("cinfo")) {
			handleClientInfoMessage(cmd);
		} else if(cmd.cmd == QStringLiteral("lookup")) {
			handleLookupMessage(cmd);
		} else {
			sendError(
				QStringLiteral("clientLookupUnsupported"),
				QStringLiteral(
					"Your version of Drawpile is too old. To update, go to "
					"drawpile.net and download a newer version."));
		}

	} else if(m_state == State::WaitForIdent) {
		// Wait for user identification before moving on to session listing
		if(cmd.cmd == QStringLiteral("cinfo")) {
			handleClientInfoMessage(cmd);
		} else if(cmd.cmd == QStringLiteral("lookup")) {
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
			m_state = State::Ignore;
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message", cmd.cmd);
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
			m_state = State::Ignore;
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message", cmd.cmd);
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

void LoginHandler::handleClientInfoMessage(const net::ServerCommand &cmd)
{
	if(!m_clientInfo.isEmpty()) {
		sendError(
			QStringLiteral("cinfoAlreadyDone"),
			QStringLiteral("Client info already done"));
		return;
	}

	if(cmd.args.size() != 2 || !cmd.args[0].isString() ||
	   !cmd.args[1].isObject()) {
		sendError(
			QStringLiteral("syntax"),
			QStringLiteral("Expected host name and client info object"));
		return;
	}

	ClientInfoLogGuard clientInfoLogGuard(
		this, extractClientInfo(cmd.args[1].toObject(), false));

	static QRegularExpression versionRe(
		QStringLiteral("\\A([0-9]+)\\.([0-9+])\\.([0-9]+)"));
	QString clientVersion = clientInfoLogGuard.appVersion();
	if(!versionRe.match(clientVersion).hasMatch()) {
		sendError(
			QStringLiteral("badversion"),
			QStringLiteral("Invalid client version"));
		return;
	}

	if(!verifySystemId(clientInfoLogGuard.sid(), true)) {
		return;
	}

	QString expectedHost = m_config->internalConfig().normalizedHostname;
	if(!expectedHost.isEmpty()) {
		QString host = cmd.args[0].toString();
		if(expectedHost.compare(host, Qt::CaseInsensitive) != 0) {
			m_client->log(
				Log()
					.about(
						m_requireMatchingHost ? Log::Level::Error
											  : Log::Level::Warn,
						Log::Topic::RuleBreak)
					.message(
						QStringLiteral(
							"Client host name '%1' is not our host name '%2'")
							.arg(host, expectedHost)));
			if(m_requireMatchingHost) {
				sendError(
					QStringLiteral("mismatchedHostName"),
					QStringLiteral("Invalid host name."));
				return;
			}
		}
	}

	if(m_state == State::WaitForClientInfo) {
		m_state = needsLookup() ? State::WaitForLookup : State::WaitForIdent;
	}

	m_clientInfo = clientInfoLogGuard.info();
	send(net::ServerReply::makeResultClientInfo(
		QStringLiteral("Client info OK!"), m_client->isBrowser()));
}

void LoginHandler::handleLookupMessage(const net::ServerCommand &cmd)
{
	if(!m_lookup.isNull()) {
		sendError(
			QStringLiteral("lookupAlreadyDone"),
			QStringLiteral("Session lookup already done"));
		return;
	}

	compat::sizetype argc = cmd.args.size();
	net::Message msg;
	if(argc == 0) {
		m_lookup = QStringLiteral("");
		msg = net::ServerReply::makeResultHostLookup(
			QStringLiteral("Host lookup OK!"));

	} else if(argc == 1) {
		QString sessionIdOrAlias = cmd.args[0].toString();
		QString inviteSecret;
		if(int pos = sessionIdOrAlias.lastIndexOf(':'); pos != -1) {
			inviteSecret = sessionIdOrAlias.mid(pos + 1);
			sessionIdOrAlias.truncate(pos);
		}

		if(sessionIdOrAlias.isEmpty()) {
			if(m_mandatoryLookup) {
				sendError(
					QStringLiteral("lookupRequired"),
					QStringLiteral("This server only allows joining sessions "
								   "through a direct link."));
				return;
			}
			m_lookup = QStringLiteral("");
			msg = net::ServerReply::makeResultJoinLookup(
				QStringLiteral("Empty join lookup OK!"), QJsonObject());

		} else {
			Sessions::JoinResult jr = m_sessions->checkSessionJoin(
				m_client, sessionIdOrAlias, inviteSecret);
			if(jr.id.isEmpty()) {
				sendError(
					"lookupFailed",
					QStringLiteral(
						"Session not found, it may have ended or its "
						"invite link has changed"));
				return;
			} else {
				switch(jr.invite) {
				case Sessions::JoinResult::InviteOk:
					break;
				case Sessions::JoinResult::InviteNotFound:
					sendError(
						"inviteNotFound",
						QStringLiteral(
							"Invite is invalid or has been revoked"));
					return;
				case Sessions::JoinResult::InviteLimitReached:
					sendError(
						"inviteLimitReached",
						QStringLiteral("Invite has already been used up"));
					return;
				}
			}

			m_lookup = jr.id;
			m_lookupInviteSecret = inviteSecret;
			msg = net::ServerReply::makeResultJoinLookup(
				QStringLiteral("Join lookup OK!"), jr.description);
		}

	} else {
		sendError(
			QStringLiteral("syntax"),
			QStringLiteral("Expected optional session id or alias"));
		return;
	}

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
		m_client->log(
			Log()
				.about(Log::Level::Warn, Log::Topic::RuleBreak)
				.message(QStringLiteral("Attempt to use invalid username '%1'")
							 .arg(username)));
		sendError("badUsername", "Invalid username");
		return;
	}

	if(m_config->isNameBanned(username)) {
		m_client->log(Log()
						  .about(Log::Level::Warn, Log::Topic::RuleBreak)
						  .message(QStringLiteral(
									   "Attempt to use forbidden username '%1'")
									   .arg(username)));
		sendError(
			QStringLiteral("forbiddenUsername"),
			QStringLiteral("Forbidden username"));
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
					extAuthMod, m_config->getConfigBool(config::ExtAuthHost),
					m_config->getConfigBool(config::EnableGhosts) &&
						m_config->getConfigBool(config::ExtAuthGhosts),
					extAuthBanExempt,
					m_config->getConfigBool(config::ExtAuthWeb),
					m_config->getConfigBool(config::ExtAuthWebHost),
					m_config->getConfigBool(config::ExtAuthWebSession),
					m_config->getConfigBool(config::ExtAuthPersist));

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
			++m_authPasswordAttempts;
			m_client->log(
				Log()
					.about(Log::Level::Warn, Log::Topic::RuleBreak)
					.message(
						QStringLiteral(
							"Incorrect password for account %1 (attempt %2/%3)")
							.arg(userAccount.username)
							.arg(m_authPasswordAttempts)
							.arg(MAX_PASSWORD_ATTEMPTS)));
			sendError(
				"badPassword", "Incorrect password",
				m_authPasswordAttempts >= MAX_PASSWORD_ATTEMPTS);
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
			m_config->getConfigBool(config::EnableGhosts), true, true, true,
			true, true);
		break;
	}
}

void LoginHandler::authLoginOk(
	const QString &username, const QString &authId, const QStringList &flags,
	const QByteArray &avatar, bool allowMod, bool allowHost, bool allowGhost,
	bool allowBanExempt, bool allowWeb, bool allowWebHost, bool allowWebSession,
	bool allowPersist)
{
	Q_ASSERT(!authId.isEmpty());

	bool isMod = false;
	bool wantGhost = false;
	QSet<QString> effectiveFlags;
	for(const QString &flag : flags) {
		if(!effectiveFlags.contains(flag)) {
			bool shouldInsert;
			if(flag == QStringLiteral("MOD")) {
				shouldInsert = allowMod;
				isMod = allowMod;
			} else if(flag == QStringLiteral("GHOST")) {
				shouldInsert = allowGhost;
				wantGhost = true;
			} else if(flag == QStringLiteral("HOST")) {
				shouldInsert = allowHost;
			} else if(flag == QStringLiteral("BANEXEMPT")) {
				shouldInsert = allowBanExempt;
			} else if(flag == QStringLiteral("WEB")) {
				shouldInsert = allowWeb;
			} else if(flag == QStringLiteral("WEBHOST")) {
				shouldInsert = allowWebHost;
			} else if(flag == QStringLiteral("WEBSESSION")) {
				shouldInsert = allowWebSession;
			} else if(flag == QStringLiteral("PERSIST")) {
				shouldInsert = allowPersist;
			} else {
				shouldInsert = true;
			}

			if(shouldInsert) {
				effectiveFlags.insert(flag);
			}
		}
	}

	insertImplicitFlags(effectiveFlags);

	if(m_client->isBrowser() &&
	   !effectiveFlags.contains(QStringLiteral("WEB"))) {
		sendError(
			QStringLiteral("webNotAllowed"),
			QStringLiteral(
				"You don't have permission to connect from the web."));
		return;
	}

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
	m_client->setAuthFlags(effectiveFlags);

	m_client->setModerator(isMod, isGhost);
	if(!avatar.isEmpty()) {
		m_client->setAvatar(avatar);
	}

	m_hostPrivilege = getHostPrivilege(effectiveFlags);

	if(m_client->triggerBan(true)) {
		m_state = State::Ignore;
		return;
	} else {
		m_state = State::WaitForLogin;
	}

	send(net::ServerReply::makeResultLoginOk(
		QStringLiteral("Authenticated login OK!"), QStringLiteral("identOk"),
		flagSetToJson(effectiveFlags), m_client->username(), false));
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

	QSet<QString> effectiveFlags;
	insertImplicitFlags(effectiveFlags);
	m_client->setAuthFlags(effectiveFlags);
	m_hostPrivilege = getHostPrivilege(effectiveFlags);

	QJsonArray jsonFlags;
	for(const QString &flag : effectiveFlags) {
		jsonFlags.append(flag);
	}

	send(net::ServerReply::makeResultLoginOk(
		QStringLiteral("Guest login OK!"), QStringLiteral("identOk"), jsonFlags,
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

	ClientInfoLogGuard clientInfoLogGuard(
		this, extractClientInfo(cmd.kwargs, true));
	if(!compareClientInfo(clientInfoLogGuard.info())) {
		return;
	}

	// Basic validation
	if(!m_lookup.isEmpty()) {
		sendError(
			QStringLiteral("lookupHostMismatch"),
			QStringLiteral("Cannot host when a joining intent was given"));
		return;
	}

	QString password = cmd.kwargs.value(QStringLiteral("password")).toString();
	switch(m_hostPrivilege) {
	case HostPrivilege::Any:
		break;
	case HostPrivilege::Passworded:
		if(password.isEmpty()) {
			sendError(
				QStringLiteral("passwordedHost"),
				QStringLiteral("You're not allowed to host sessions without a "
							   "session password"));
			return;
		}
		break;
	default:
		Q_ASSERT(m_hostPrivilege == HostPrivilege::None);
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
	if(!m_client->isModerator() && m_minimumProtocolVersion.isValid()) {
		if(protocolVersion.ns() != m_minimumProtocolVersion.ns()) {
			sendError(
				QStringLiteral("protoverns"),
				QStringLiteral("Mismatched protocol namespace, minimum "
							   "protocol version is %1")
					.arg(m_minimumProtocolVersionString));
			return;
		} else if(!protocolVersion.isGreaterOrEqual(m_minimumProtocolVersion)) {
			sendError(
				QStringLiteral("protoverold"),
				QStringLiteral(
					"Your protocol version is too old, the minimum on "
					"this server is %1. That probably means your "
					"version of Drawpile is too old. To update, go to "
					"drawpile.net and download a newer version.")
					.arg(m_minimumProtocolVersionString));
			return;
		}
	}

	if(!verifySystemId(
		   clientInfoLogGuard.sid(), protocolVersion.shouldHaveSystemId())) {
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

	if(!password.isEmpty()) {
		session->history()->setPassword(password);
	}

	SessionHistory::Flags flags;
	if(shouldAllowWebOnHost(cmd, session)) {
		flags.setFlag(SessionHistory::AllowWeb);
	}

	if(m_config->getConfigBool(config::Invites)) {
		flags.setFlag(SessionHistory::Invites);
	}

	QString unlist = m_config->getConfigString(config::UnlistedHostPolicy);
	if(unlist.contains(QStringLiteral("ALL")) ||
	   (unlist.contains(QStringLiteral("WEB")) && m_client->isBrowser())) {
		flags.setFlag(SessionHistory::Unlisted);
	}

	if(flags) {
		SessionHistory *history = session->history();
		history->setFlags(history->flags() | flags);
	}

	// Mark login phase as complete.
	// No more login messages will be sent to this user.
	send(net::ServerReply::makeResultJoinHost(
		QStringLiteral("Starting new session!"), QStringLiteral("host"),
		{{QStringLiteral("id"),
		  sessionAlias.isEmpty() ? session->id() : sessionAlias},
		 {QStringLiteral("user"), userId},
		 {QStringLiteral("flags"), sessionFlags(session)},
		 {QStringLiteral("authId"), m_client->authId()}}));

	checkClientCapabilities(cmd);

	m_complete = true;
	session->joinUser(m_client, true);

	deleteLater();
}

void LoginHandler::handleJoinMessage(const net::ServerCommand &cmd)
{
	Q_ASSERT(!m_client->username().isEmpty());

	ClientInfoLogGuard clientInfoLogGuard(
		this, extractClientInfo(cmd.kwargs, true));
	if(!compareClientInfo(clientInfoLogGuard.info())) {
		return;
	}

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
	Session *session = m_sessions->getSessionById(sessionId, true);
	if(!session) {
		sendError("notFound", "Session not found!");
		return;
	}

	if(!m_lookup.isEmpty() && session->id() != m_lookup) {
		sendError(
			QStringLiteral("lookupMismatch"),
			QStringLiteral("Cannot look up one session and then join another"));
		return;
	}

	if(!verifySystemId(
		   clientInfoLogGuard.sid(),
		   session->history()->protocolVersion().shouldHaveSystemId())) {
		return;
	}

	Invite *invite = nullptr;
	if(!m_lookupInviteSecret.isEmpty()) {
		switch(session->checkInvite(
			m_client, m_lookupInviteSecret, true, &invite)) {
		case CheckInviteResult::InviteOk:
		case CheckInviteResult::InviteUsed:
		case CheckInviteResult::AlreadyInvited:
		case CheckInviteResult::AlreadyInvitedNameChanged:
			break;
		case CheckInviteResult::NoClientKey:
			sendError(
				"inviteNoClientKey",
				QStringLiteral("No client key to use with invite"));
			return;
		case CheckInviteResult::NotFound:
			sendError(
				"inviteNotFound",
				QStringLiteral("Invite is invalid or has been revoked"));
			return;
		case CheckInviteResult::MaxUsesReached:
			sendError(
				"inviteLimitReached",
				QStringLiteral("Invite has already been used up"));
			return;
		}
	}

	QString password = cmd.kwargs.value(QStringLiteral("password")).toString();
	if(!invite && !password.isEmpty()) {
		switch(session->checkInvite(m_client, password, true, &invite)) {
		case CheckInviteResult::InviteOk:
		case CheckInviteResult::InviteUsed:
		case CheckInviteResult::AlreadyInvited:
		case CheckInviteResult::AlreadyInvitedNameChanged:
			break;
		case CheckInviteResult::NoClientKey:
		case CheckInviteResult::NotFound:
			invite = nullptr;
			break;
		case CheckInviteResult::MaxUsesReached:
			sendError(
				"inviteLimitReached",
				QStringLiteral("Invite has already been used up"));
			return;
		}
	}

	if(invite) {
		m_client->setInviteSecret(invite->secret);
	}

	SessionHistory *history = session->history();
	if(!invite && m_client->isBrowser() &&
	   !history->hasFlag(SessionHistory::AllowWeb)) {
		sendError(
			QStringLiteral("noWebJoin"),
			QStringLiteral("This session does not allow joining from the web"));
		return;
	}

	if(!m_client->isModerator()) {
		// Non-moderators have to obey access restrictions
		int banId = history->banlist().isBanned(
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
		if(!invite && session->isClosed()) {
			sendError("closed", "This session is closed");
			return;
		}
		if(!invite && history->hasFlag(SessionHistory::AuthOnly) &&
		   !m_client->isAuthenticated()) {
			sendError("authOnly", "This session does not allow guest logins");
			return;
		}

		if(!invite && !history->checkPassword(password)) {
			++m_sessionPasswordAttempts;
			m_client->log(
				Log()
					.about(Log::Level::Warn, Log::Topic::RuleBreak)
					.message(
						QStringLiteral(
							"Incorrect password for session %1 (attempt %2/%3)")
							.arg(session->id())
							.arg(m_sessionPasswordAttempts)
							.arg(MAX_PASSWORD_ATTEMPTS)));
			sendError(
				"badPassword", "Incorrect password",
				m_sessionPasswordAttempts >= MAX_PASSWORD_ATTEMPTS);
			return;
		}
	}

	Client *existingClient = session->getClientByUsername(m_client->username());
	if(existingClient) {
		bool shouldReplace =
			(m_client->isAuthenticated() && existingClient->isAuthenticated() &&
			 m_client->authId() == existingClient->authId()) ||
			(m_client->hasSid() && existingClient->hasSid() &&
			 m_client->sid() == existingClient->sid());
		if(!shouldReplace) {
			sendError("nameInuse", "This username is already in use");
			return;
		}
	}

	if(m_client->triggerBan(false)) {
		m_state = State::Ignore;
		return;
	}

	if(existingClient) {
		m_client->setId(existingClient->id());
	} else {
		session->assignId(m_client);
	}

	send(net::ServerReply::makeResultJoinHost(
		QStringLiteral("Joining a session!"), QStringLiteral("join"),
		{{QStringLiteral("id"), session->aliasOrId()},
		 {QStringLiteral("user"), m_client->id()},
		 {QStringLiteral("flags"), sessionFlags(session)},
		 {QStringLiteral("authId"), m_client->authId()}}));

	checkClientCapabilities(cmd);

	m_complete = true;
	session->joinUser(m_client, false, invite);

	deleteLater();
}

void LoginHandler::checkClientCapabilities(const net::ServerCommand &cmd)
{
	const QString capabilities =
		cmd.kwargs[QStringLiteral("capabilities")].toString();
	if(capabilities.contains(QStringLiteral("KEEPALIVE"))) {
		m_client->setKeepAliveTimeout(30 * 1000);
	}
}

QJsonObject
LoginHandler::extractClientInfo(const QJsonObject &o, bool checkAuthenticated)
{
	QJsonObject info;
	for(const QString &key : clientInfoKeys) {
		if(o.contains(key)) {
			QJsonValue value = o.value(key);
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
	if(checkAuthenticated && m_client->isAuthenticated()) {
		info[QStringLiteral("auth_id")] = m_client->authId();
	}

	return info;
}

bool LoginHandler::compareClientInfo(const QJsonObject &info)
{
	if(!m_clientInfo.isEmpty()) {
		for(const QString &key : clientInfoKeys) {
			if(info.value(key) != m_clientInfo.value(key)) {
				m_client->log(
					Log()
						.about(Log::Level::Error, Log::Topic::RuleBreak)
						.message(QStringLiteral("Mismatched client info '%1'")
									 .arg(key)));
				m_state = State::Ignore;
				m_client->disconnectClient(
					Client::DisconnectionReason::Error, "invalid message",
					QStringLiteral("mismatched client info"));
				return false;
			}
		}
	}
	return true;
}

void LoginHandler::logClientInfo(const QJsonObject &info)
{
	bool infoChanged =
		m_lastLoggedClientInfo.isEmpty() || info != m_lastLoggedClientInfo;
	if(infoChanged) {
		m_lastLoggedClientInfo = info;
	}

	Session *session = m_client->session();
	bool sessionChanged = m_lastClientSession != session;
	if(sessionChanged) {
		m_lastClientSession = session;
	}

	if(infoChanged || sessionChanged) {
		m_client->log(Log()
						  .about(Log::Level::Info, Log::Topic::ClientInfo)
						  .message(QJsonDocument(m_lastLoggedClientInfo)
									   .toJson(QJsonDocument::Compact)));
	}
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
	m_state = m_requireMatchingHost ? State::WaitForClientInfo
			  : needsLookup()		? State::WaitForLookup
									: State::WaitForIdent;
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
			Client::DisconnectionReason::Error, "Login error", code);
	}
}

bool LoginHandler::needsLookup() const
{
	return m_mandatoryLookup ||
		   (m_minimumProtocolVersion.isValid() &&
			m_minimumProtocolVersion.shouldSupportLookup());
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

bool LoginHandler::verifySystemId(const QString &sid, bool required)
{
	m_client->setSid(sid);
	if(sid.isEmpty()) {
		if(required) {
			m_client->log(Log()
							  .about(Log::Level::Error, Log::Topic::RuleBreak)
							  .message(QStringLiteral("Missing required sid")));
			m_state = State::Ignore;
			m_client->disconnectClient(
				Client::DisconnectionReason::Error, "invalid message",
				QStringLiteral("missing required sid"));
			return false;
		}
	} else if(!isValidSid(sid)) {
		m_client->log(Log()
						  .about(Log::Level::Error, Log::Topic::RuleBreak)
						  .message(QStringLiteral("Invalid sid %1").arg(sid)));
		m_state = State::Ignore;
		m_client->disconnectClient(
			Client::DisconnectionReason::Error, "invalid message",
			QStringLiteral("invalid sid"));
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

void LoginHandler::insertImplicitFlags(QSet<QString> &effectiveFlags)
{
	std::pair<QString, server::ConfigKey> implicitFlags[] = {
		{QStringLiteral("HOST"), config::AllowGuestHosts},
		{QStringLiteral("WEB"), config::AllowGuestWeb},
		{QStringLiteral("WEBSESSION"), config::AllowGuestWebSession},
		{QStringLiteral("WEBHOST"), config::AllowGuestWebHosts},
	};
	for(const auto &[flag, cfg] : implicitFlags) {
		if(!effectiveFlags.contains(flag) && m_config->getConfigBool(cfg)) {
			effectiveFlags.insert(flag);
		}
	}
	// Moderators can always persist sessions, so this flag isn't strictly
	// necessary, but it tells the client that we're a server that supports
	// overriding persistence even if the flag isn't set on a server level.
	if(effectiveFlags.contains(QStringLiteral("MOD"))) {
		effectiveFlags.insert(QStringLiteral("PERSIST"));
	}
}

QJsonArray LoginHandler::flagSetToJson(const QSet<QString> &flags)
{
	QJsonArray json;
	for(const QString &flag : flags) {
		json.append(flag);
	}
	return json;
}

LoginHandler::HostPrivilege
LoginHandler::getHostPrivilege(const QSet<QString> &effectiveFlags) const
{
	if(effectiveFlags.contains(QStringLiteral("HOST"))) {
		if(m_client->isBrowser()) {
			if(effectiveFlags.contains(QStringLiteral("WEBHOST"))) {
				if(!effectiveFlags.contains(QStringLiteral("WEBSESSION")) &&
				   m_config->getConfigBool(
					   config::PasswordDependentWebSession)) {
					return HostPrivilege::Passworded;
				} else {
					return HostPrivilege::Any;
				}
			}
		} else {
			return HostPrivilege::Any;
		}
	}
	return HostPrivilege::None;
}

bool LoginHandler::haveHostPrivilege(bool passworded) const
{
	switch(m_hostPrivilege) {
	case HostPrivilege::Passworded:
		return passworded;
	case HostPrivilege::Any:
		return true;
	default:
		Q_ASSERT(m_hostPrivilege == HostPrivilege::None);
		return false;
	}
}

bool LoginHandler::shouldAllowWebOnHost(
	const net::ServerCommand &cmd, const Session *session) const
{
	// If the client is hosting via browser, the session must allow them.
	// Otherwise they wouldn't be able to rejoin it if they leave.
	if(m_client->isBrowser()) {
		return true;
	}
	// If the client has the WEBSESSION flag, they may explicitly specify what
	// they want the browser allowance to be.
	QString webKey = QStringLiteral("web");
	bool canManageWebSession = m_client->canManageWebSession();
	if(canManageWebSession && cmd.kwargs.contains(webKey)) {
		QJsonValue webValue = cmd.kwargs[webKey];
		if(webValue.isBool()) {
			return webValue.toBool();
		}
	}
	// If password-dependent browser allowance is enabled, follow that.
	if(m_config->getConfigBool(config::PasswordDependentWebSession)) {
		return !session->history()->passwordHash().isEmpty();
	}
	// Otherwise make it dependent on the client's abilities.
	return canManageWebSession;
}

}
