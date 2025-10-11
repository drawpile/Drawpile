// SPDX-License-Identifier: GPL-3.0-or-later
#include "thinsrv/multiserver.h"
#include "cmake-config/config.h"
#include "libserver/jsonapi.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libserver/sessionserver.h"
#include "libserver/thinserverclient.h"
#include "libshared/util/whatismyip.h"
#include "thinsrv/database.h"
#include "thinsrv/extbans.h"
#include "thinsrv/initsys.h"
#include "thinsrv/templatefiles.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>
#include <QWebSocket>
#include <QWebSocketServer>

namespace server {

MultiServer::MultiServer(ServerConfig *config, QObject *parent)
	: QObject(parent)
	, m_config(config)
	, m_tcpServer(nullptr)
	, m_webSocketServer(nullptr)
	, m_extBans(new ExtBans(config, this))
	, m_state(STOPPED)
	, m_autoStop(false)
	, m_port(0)
{
	m_config->setParent(this);
	m_sessions = new SessionServer(config, this);
	m_started = QDateTime::currentDateTimeUtc();

	Database *db = qobject_cast<Database *>(m_config);
	if(db) {
		db->loadExternalIpBans(m_extBans);
	}

	connect(this, &MultiServer::serverStarted, m_extBans, &ExtBans::start);
	connect(this, &MultiServer::serverStopped, m_extBans, &ExtBans::stop);

	connect(
		m_sessions, &SessionServer::sessionCreated, this,
		&MultiServer::assignRecording);
	connect(
		m_sessions, &SessionServer::sessionEnded, this,
		&MultiServer::tryAutoStop);
	connect(m_sessions, &SessionServer::userCountChanged, [this](int users) {
		printStatusUpdate();
		emit userCountChanged(users);
		// The server will be fully stopped after all users have disconnected
		if(users == 0) {
			if(m_state == STOPPING) {
				stop();
			} else {
				tryAutoStop();
			}
		}
	});
}

/**
 * @brief Automatically stop server when last session is closed
 *
 * This is used in socket activation mode. The server will be restarted
 * by the system init daemon when needed again.
 * @param autostop
 */
void MultiServer::setAutoStop(bool autostop)
{
	m_autoStop = autostop;
}

void MultiServer::setRecordingPath(const QString &path)
{
	m_recordingPath = path;
}

void MultiServer::setSessionDirectory(const QDir &path)
{
	m_sessions->setSessionDir(path);
}

void MultiServer::setTemplateDirectory(const QDir &dir)
{
	const TemplateLoader *old = m_sessions->templateLoader();
	TemplateFiles *loader = new TemplateFiles(dir, m_sessions);
	m_sessions->setTemplateLoader(loader);
	delete old;
}

bool MultiServer::createServer()
{
	return createServer(false);
}

bool MultiServer::createServer(bool enableWebSockets)
{
	if(!m_sslCertFile.isEmpty() && !m_sslKeyFile.isEmpty()) {
		SslServer *server =
			new SslServer(m_sslCertFile, m_sslKeyFile, m_sslKeyAlgorithm, this);
		if(!server->isValidCert()) {
			emit serverStartError("Couldn't load TLS certificate");
			return false;
		}
		m_tcpServer = server;

	} else {
		m_tcpServer = new QTcpServer(this);
	}

	connect(
		m_tcpServer, &QTcpServer::newConnection, this,
		&MultiServer::newTcpClient);

	if(enableWebSockets) {
		// TODO: Allow running a TLS-secured WebSocket server. Currently, this
		// instead requires a reverse proxy server like nginx or something. The
		// user almost certainly has one of those anyway though, so it's fine.
		m_webSocketServer = new QWebSocketServer(
			QStringLiteral("drawpile-srv_%1").arg(cmake_config::version()),
			QWebSocketServer::NonSecureMode, this);
		connect(
			m_webSocketServer, &QWebSocketServer::newConnection, this,
			&MultiServer::newWebSocketClient);
	}

	return true;
}

bool MultiServer::abortStart()
{
	delete m_tcpServer;
	m_tcpServer = nullptr;
	delete m_webSocketServer;
	m_webSocketServer = nullptr;
	m_state = STOPPED;
	return false;
}

void MultiServer::updateInternalConfig()
{
	InternalConfig icfg = m_config->internalConfig();
	icfg.realPort = m_port;
	icfg.webSocket = m_webSocketServer != nullptr;
	m_config->setInternalConfig(icfg);
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param address listening address
 * @return true on success
 */
bool MultiServer::start(
	quint16 tcpPort, const QHostAddress &tcpAddress, quint16 webSocketPort,
	const QHostAddress &webSocketAddress)
{
	Q_ASSERT(m_state == STOPPED);
	m_state = RUNNING;
	if(!createServer(webSocketPort != 0)) {
		return abortStart();
	}

	if(!m_tcpServer->listen(tcpAddress, tcpPort)) {
		emit serverStartError(m_tcpServer->errorString());
		m_sessions->config()->logger()->logMessage(
			Log()
				.about(Log::Level::Error, Log::Topic::Status)
				.message(m_tcpServer->errorString()));
		return abortStart();
	}

	if(m_webSocketServer &&
	   !m_webSocketServer->listen(webSocketAddress, webSocketPort)) {
		emit serverStartError(m_webSocketServer->errorString());
		m_sessions->config()->logger()->logMessage(
			Log()
				.about(Log::Level::Error, Log::Topic::Status)
				.message(m_webSocketServer->errorString()));
		return abortStart();
	}

	m_port = m_tcpServer->serverPort();
	updateInternalConfig();

	emit serverStarted();
	m_sessions->config()->logger()->logMessage(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(QString("Started listening for TCP connections on port %1 "
							 "at address %2")
						 .arg(tcpPort)
						 .arg(tcpAddress.toString())));
	if(m_webSocketServer) {
		m_sessions->config()->logger()->logMessage(
			Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(QString("Started listening for WebSocket connections "
								 "on port %1 at address %2")
							 .arg(webSocketPort)
							 .arg(webSocketAddress.toString())));
	}
	return true;
}

void MultiServer::assignRecording(Session *session)
{
	QString filename = m_recordingPath;

	if(filename.isEmpty())
		return;

	// Expand home directory
	if(filename.startsWith("~/")) {
		filename = QString(qgetenv("HOME")) + filename.mid(1);
	}

	// Use default file pattern if target is a directory
	QFileInfo fi(filename);
	if(fi.isDir()) {
		filename = QFileInfo(QDir(filename), "%d %t session %i.dprec")
					   .absoluteFilePath();
	}

	// Expand placeholders
	QDateTime now = QDateTime::currentDateTime();
	filename.replace("%d", now.toString("yyyy-MM-dd"));
	filename.replace("%t", now.toString("HH.mm.ss"));
	filename.replace("%i", session->id());
	filename.replace("%a", session->aliasOrId());

	fi = QFileInfo(filename);

	if(!fi.absoluteDir().mkpath(".")) {
		qWarning(
			"Recording directory \"%s\" does not exist and cannot be created!",
			qPrintable(fi.absolutePath()));
	} else {
		session->setRecordingFile(fi.absoluteFilePath());
	}
}

void MultiServer::newTcpClient()
{
	QTcpSocket *tcpSocket = m_tcpServer->nextPendingConnection();
	m_sessions->config()->logger()->logMessage(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.user(0, tcpSocket->peerAddress(), QString())
			.message(QStringLiteral("New TCP client connected")));
	newClient(new ThinServerClient(tcpSocket, m_sessions->config()->logger()));
}

void MultiServer::newWebSocketClient()
{
	QWebSocket *webSocket = m_webSocketServer->nextPendingConnection();

	QHostAddress ip;
	QString ipSource;
	QHostAddress peerAddress = webSocket->peerAddress();
	QNetworkRequest request = webSocket->request();
	QString origin = QString::fromUtf8(request.rawHeader("Origin"));
	if(request.hasRawHeader("X-Real-IP") &&
	   ip.setAddress(QString::fromUtf8(request.rawHeader("X-Real-IP")))) {
		ipSource = QStringLiteral("X-Real-IP header");
	} else {
		ip = peerAddress;
		ipSource = QStringLiteral("WebSocket peer address");
	}

	m_sessions->config()->logger()->logMessage(
		Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.user(0, ip, QString())
			.message(QStringLiteral("New WebSocket client connected from %1 "
									"(IP from %2, Origin '%3')")
						 .arg(peerAddress.toString(), ipSource, origin)));
	newClient(new ThinServerClient(
		webSocket, ip, !origin.isEmpty(), m_sessions->config()->logger()));
}

void MultiServer::newClient(ThinServerClient *client)
{
	client->applyBan(m_config->isAddressBanned(client->peerAddress()));
	m_sessions->addClient(client);
	printStatusUpdate();
}

void MultiServer::printStatusUpdate()
{
	initsys::notifyStatus(QString("%1 users and %2 sessions")
							  .arg(m_sessions->totalUsers())
							  .arg(m_sessions->sessionCount()));
}

/**
 * @brief Stop the server if vacant (and autostop is enabled)
 */
void MultiServer::tryAutoStop()
{
	if(m_state == RUNNING && m_autoStop && m_sessions->sessionCount() == 0 &&
	   m_sessions->totalUsers() == 0) {
		m_sessions->config()->logger()->logMessage(
			Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message("Autostopping due to lack of sessions."));
		stop();
	}
}

/**
 * Disconnect all clients and stop listening.
 */
void MultiServer::stop()
{
	if(m_state == RUNNING) {
		m_sessions->config()->logger()->logMessage(
			Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message(QString("Stopping server and kicking out %1 users...")
							 .arg(m_sessions->totalUsers())));

		m_state = STOPPING;
		m_tcpServer->close();
		if(m_webSocketServer) {
			m_webSocketServer->close();
		}
		m_port = 0;

		m_sessions->stopAll();
	}

	if(m_state == STOPPING) {
		if(m_sessions->totalUsers() == 0) {
			m_state = STOPPED;
			delete m_tcpServer;
			m_tcpServer = nullptr;
			delete m_webSocketServer;
			m_webSocketServer = nullptr;
			m_sessions->config()->logger()->logMessage(
				Log()
					.about(Log::Level::Info, Log::Topic::Status)
					.message("Server stopped."));
			emit serverStopped();
		}
	}
}

JsonApiResult MultiServer::callJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	QString head;
	QStringList tail;
	std::tie(head, tail) = popApiPath(path);

	if(head == QStringLiteral("server")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("server"),
			&MultiServer::serverJsonApi);
	} else if(head == QStringLiteral("status")) {
		return statusJsonApi(method, tail, request);
	} else if(head == QStringLiteral("sessions")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("sessions"),
			&MultiServer::sessionsJsonApi);
	} else if(head == QStringLiteral("users")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("sessions"),
			&MultiServer::usersJsonApi);
	} else if(head == QStringLiteral("banlist")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("bans"),
			&MultiServer::banlistJsonApi);
	} else if(head == QStringLiteral("systembans")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("bans"),
			&MultiServer::systembansJsonApi);
	} else if(head == QStringLiteral("userbans")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("bans"),
			&MultiServer::userbansJsonApi);
	} else if(head == QStringLiteral("listserverwhitelist")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("listserverwhitelist"),
			&MultiServer::listserverWhitelistJsonApi);
	} else if(head == QStringLiteral("accounts")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("accounts"),
			&MultiServer::accountsJsonApi);
	} else if(head == QStringLiteral("log")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("log"),
			&MultiServer::logJsonApi);
	} else if(head == QStringLiteral("extbans")) {
		return callJsonApiCheckLock(
			method, tail, request, QStringLiteral("extbans"),
			&MultiServer::extbansJsonApi, {{QStringLiteral("refresh")}});
	} else if(head == QStringLiteral("locks")) {
		return locksJsonApi(method, tail, request);
	} else {
		return JsonApiNotFound();
	}
}

void MultiServer::callJsonApiAsync(
	const QString &requestId, JsonApiMethod method, const QStringList &path,
	const QJsonObject &request)
{
	JsonApiResult result = callJsonApi(method, path, request);
	emit jsonApiResult(requestId, result);
}

JsonApiResult MultiServer::callJsonApiCheckLock(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	const QString &section,
	const std::function<JsonApiResult(
		MultiServer *, JsonApiMethod, const QStringList &, const QJsonObject &,
		bool)> &fn,
	const QSet<QStringList> &allowedPaths)
{
	bool sectionLocked = m_config->isAdminSectionLocked(section);
	if(method == JsonApiMethod::Get || !sectionLocked ||
	   allowedPaths.contains(path)) {
		return fn(this, method, path, request, sectionLocked);
	} else {
		return JsonApiErrorResult(
			JsonApiResult::Forbidden, QStringLiteral("This section is locked"));
	}
}

JsonApiResult MultiServer::serverJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method != JsonApiMethod::Get && method != JsonApiMethod::Update) {
		return JsonApiBadMethod();
	}

	const ConfigKey settings[] = {
		config::ClientTimeout,
		config::SessionSizeLimit,
		config::AutoresetThreshold,
		config::SessionCountLimit,
		config::EnablePersistence,
		config::ArchiveMode,
		config::IdleTimeLimit,
		config::ServerTitle,
		config::WelcomeMessage,
		config::AllowGuestHosts,
		config::AllowGuests,
#ifdef HAVE_LIBSODIUM
		config::UseExtAuth,
		config::ExtAuthKey,
		config::ExtAuthGroup,
		config::ExtAuthFallback,
		config::ExtAuthMod,
		config::ExtAuthHost,
		config::ExtAuthAvatars,
		config::ExtAuthBanExempt,
		config::ExtAuthGhosts,
		config::ExtAuthPersist,
		config::ExtAuthWebHost,
#endif
		config::LogPurgeDays,
		config::AllowCustomAvatars,
		config::AbuseReport,
		config::ReportToken,
		config::ForceNsfm,
		config::ExtBansUrl,
		config::ExtBansCheckInterval,
		config::AllowIdleOverride,
		config::LoginInfoUrl,
		config::EnableGhosts,
		config::RuleText,
		config::MinimumProtocolVersion,
		config::MandatoryLookup,
		config::AllowGuestWeb,
		config::AllowGuestWebSession,
#ifdef HAVE_LIBSODIUM
		config::ExtAuthWeb,
		config::ExtAuthWebSession,
#endif
		config::PasswordDependentWebSession,
		config::PreferWebSockets,
		config::SessionUserLimit,
		config::EmptySessionLingerTime,
		config::RequireMatchingHost,
		config::AllowGuestWebHosts,
		config::Invites,
		config::ForbiddenNameRegex,
		config::FilterNameRegex,
		config::UnlistedHostPolicy,
		config::MinimumAutoresetThreshold,
	};
	const int settingCount = sizeof(settings) / sizeof(settings[0]);

	if(method == JsonApiMethod::Update) {
		for(int i = 0; i < settingCount; ++i) {
			if(request.contains(settings[i].name)) {
				m_config->setConfigString(
					settings[i],
					request[settings[i].name].toVariant().toString());
			}
		}
	}

	QJsonObject result;
	for(int i = 0; i < settingCount; ++i) {
		result[settings[i].name] =
			QJsonValue::fromVariant(m_config->getConfigVariant(settings[i]));
	}

	// Hide values for disabled features
	const InternalConfig &icfg = m_config->internalConfig();
	if(icfg.normalizedHostname.isEmpty()) {
		result.remove(config::RequireMatchingHost.name);
	}

	if(!icfg.reportUrl.isValid()) {
		result.remove(config::AbuseReport.name);
	}

	if(!icfg.extAuthUrl.isValid()) {
		result.remove(config::UseExtAuth.name);
	}

	if(!icfg.webSocket) {
		result.remove(config::AllowGuestWeb.name);
		result.remove(config::ExtAuthWeb.name);
		result.remove(config::AllowGuestWebSession.name);
		result.remove(config::ExtAuthWebSession.name);
		result.remove(config::PasswordDependentWebSession.name);
		result.remove(config::PreferWebSockets.name);
	}

	result.insert(QStringLiteral("_locked"), sectionLocked);
	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(result)};
}

JsonApiResult MultiServer::statusJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	Q_UNUSED(request);

	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method != JsonApiMethod::Get) {
		return JsonApiBadMethod();
	}

	QJsonObject result;
	result["started"] = m_started.toString(Qt::ISODate);
	result["sessions"] = m_sessions->sessionCount();
	result["maxSessions"] = m_config->getConfigInt(config::SessionCountLimit);
	result["users"] = m_sessions->totalUsers();
	QString localhost = m_config->internalConfig().localHostname;
	if(localhost.isEmpty())
		localhost = WhatIsMyIp::guessLocalAddress();
	result["ext_host"] = localhost;
	result["ext_port"] = m_config->internalConfig().getAnnouncePort();

	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(result)};
}

JsonApiResult MultiServer::sessionsJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	return m_sessions->callSessionJsonApi(method, path, request, sectionLocked);
}

JsonApiResult MultiServer::usersJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	return m_sessions->callUserJsonApi(method, path, request, sectionLocked);
}

JsonApiResult MultiServer::banlistJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	// Database is needed to manipulate the banlist
	Database *db = qobject_cast<Database *>(m_config);
	if(!db) {
		return JsonApiNotFound();
	}

	if(path.size() == 1) {
		if(method != JsonApiMethod::Delete) {
			return JsonApiBadMethod();
		}
		if(db->deleteIpBan(path.at(0).toInt())) {
			QJsonObject body;
			body["status"] = "ok";
			body["deleted"] = path.at(0).toInt();
			return JsonApiResult{JsonApiResult::Ok, QJsonDocument(body)};
		} else {
			return JsonApiNotFound();
		}
	}

	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		QJsonDocument body;
		if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
			body.setArray(db->getIpBanlist());
		} else {
			body.setObject({
				{QStringLiteral("bans"), db->getIpBanlist()},
				{QStringLiteral("_locked"), sectionLocked},
			});
		}
		return JsonApiResult{JsonApiResult::Ok, body};

	} else if(method == JsonApiMethod::Create) {
		QHostAddress ip{request["ip"].toString()};
		if(ip.isNull()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Valid IP address required");
		}
		int subnet = request["subnet"].toInt();
		QDateTime expiration =
			ServerConfig::parseDateTime(request["expires"].toString());
		if(expiration.isNull()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Valid expiration time required");
		}
		QString comment = request["comment"].toString();

		QJsonObject result = db->addIpBan(ip, subnet, expiration, comment);
		if(result.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::InternalError, "Database error");
		} else {
			return {JsonApiResult::Ok, QJsonDocument(result)};
		}
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult MultiServer::systembansJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	Database *db = qobject_cast<Database *>(m_config);
	if(!db) {
		return JsonApiNotFound();
	}

	if(path.size() == 1) {
		if(method != JsonApiMethod::Delete) {
			return JsonApiBadMethod();
		} else if(db->deleteSystemBan(path.at(0).toInt())) {
			QJsonObject body;
			body["status"] = "ok";
			body["deleted"] = path.at(0).toInt();
			return JsonApiResult{JsonApiResult::Ok, QJsonDocument(body)};
		} else {
			return JsonApiNotFound();
		}
	}

	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		QJsonDocument body;
		if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
			body.setArray(db->getSystemBanlist());
		} else {
			body.setObject({
				{QStringLiteral("bans"), db->getSystemBanlist()},
				{QStringLiteral("_locked"), sectionLocked},
			});
		}
		return JsonApiResult{JsonApiResult::Ok, body};

	} else if(method == JsonApiMethod::Create) {
		QString sid = request["sid"].toString();
		if(sid.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "SID required");
		}

		QDateTime expiration =
			ServerConfig::parseDateTime(request["expires"].toString());
		if(expiration.isNull()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Valid expiration time required");
		}

		BanReaction reaction =
			ServerConfig::parseReaction(request["reaction"].toString());
		if(reaction == BanReaction::Unknown ||
		   reaction == BanReaction::NotBanned) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Invalid reaction");
		}

		QString comment = request["comment"].toString();
		QString reason = request["reason"].toString();

		QJsonObject result =
			db->addSystemBan(sid, expiration, reaction, reason, comment);
		if(result.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::InternalError, "Database error");
		} else {
			return {JsonApiResult::Ok, QJsonDocument(result)};
		}
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult MultiServer::userbansJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	Database *db = qobject_cast<Database *>(m_config);
	if(!db) {
		return JsonApiNotFound();
	}

	if(path.size() == 1) {
		if(method != JsonApiMethod::Delete) {
			return JsonApiBadMethod();
		} else if(db->deleteUserBan(path.at(0).toInt())) {
			QJsonObject body;
			body["status"] = "ok";
			body["deleted"] = path.at(0).toInt();
			return JsonApiResult{JsonApiResult::Ok, QJsonDocument(body)};
		} else {
			return JsonApiNotFound();
		}
	}

	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		QJsonDocument body;
		if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
			body.setArray(db->getUserBanlist());
		} else {
			body.setObject({
				{QStringLiteral("bans"), db->getUserBanlist()},
				{QStringLiteral("_locked"), sectionLocked},
			});
		}
		return JsonApiResult{JsonApiResult::Ok, body};

	} else if(method == JsonApiMethod::Create) {
		QJsonValue rawUserId = request["userId"];
		long long userId = rawUserId.isDouble() ? rawUserId.toDouble() : 0;
		if(userId <= 0) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Valid user ID required");
		}

		QDateTime expiration =
			ServerConfig::parseDateTime(request["expires"].toString());
		if(expiration.isNull()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Valid expiration time required");
		}

		BanReaction reaction =
			ServerConfig::parseReaction(request["reaction"].toString());
		if(reaction == BanReaction::Unknown ||
		   reaction == BanReaction::NotBanned) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, "Invalid reaction");
		}

		QString comment = request["comment"].toString();
		QString reason = request["reason"].toString();

		QJsonObject result =
			db->addUserBan(userId, expiration, reaction, reason, comment);
		if(result.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::InternalError, "Database error");
		} else {
			return {JsonApiResult::Ok, QJsonDocument(result)};
		}
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult MultiServer::listserverWhitelistJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	// Database is needed to manipulate the whitelist
	Database *db = qobject_cast<Database *>(m_config);
	if(!db) {
		return JsonApiNotFound();
	}

	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Update) {
		QStringList whitelist;
		for(const auto &v : request[QStringLiteral("whitelist")].toArray()) {
			const QString str = v.toString();
			if(str.isEmpty()) {
				continue;
			}

			const QRegularExpression re(str);
			if(!re.isValid()) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest, str + ": " + re.errorString());
			}
			whitelist << str;
		}
		if(!request[QStringLiteral("enabled")].isUndefined()) {
			db->setConfigBool(
				config::AnnounceWhiteList,
				request[QStringLiteral("enabled")].toBool());
		}
		if(!request[QStringLiteral("whitelist")].isUndefined()) {
			db->updateListServerWhitelist(whitelist);
		}
	}

	const QJsonObject o{
		{QStringLiteral("enabled"),
		 db->getConfigBool(config::AnnounceWhiteList)},
		{QStringLiteral("whitelist"),
		 QJsonArray::fromStringList(db->listServerWhitelist())},
		{QStringLiteral("_locked"), sectionLocked},
	};

	return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};
}

JsonApiResult MultiServer::accountsJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	// Database is needed to manipulate account list
	Database *db = qobject_cast<Database *>(m_config);
	if(!db) {
		return JsonApiNotFound();
	}

	if(path.size() == 1) {
		QString head = path.at(0);

		if(method == JsonApiMethod::Create) {
			if(head == QStringLiteral("auth") &&
			   request.contains(QStringLiteral("username")) &&
			   request.contains(QStringLiteral("password"))) {
				RegisteredUser user = db->getUserAccount(
					request[QStringLiteral("username")].toString(),
					request[QStringLiteral("password")].toString());
				return JsonApiResult{
					JsonApiResult::Ok,
					QJsonDocument(
						QJsonObject{{QStringLiteral("status"), user.status}})};
			}

			return JsonApiNotFound();

		} else if(method == JsonApiMethod::Update) {
			QJsonObject o = db->updateAccount(head.toInt(), request);
			if(o.isEmpty()) {
				return JsonApiNotFound();
			}
			return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};

		} else if(method == JsonApiMethod::Delete) {
			if(db->deleteAccount(head.toInt())) {
				QJsonObject body;
				body[QStringLiteral("status")] = QStringLiteral("ok");
				body[QStringLiteral("deleted")] = head.toInt();
				return JsonApiResult{JsonApiResult::Ok, QJsonDocument(body)};
			} else {
				return JsonApiNotFound();
			}
		} else {
			return JsonApiBadMethod();
		}
	}

	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Get) {
		QJsonDocument body;
		if(parseRequestInt(request, QStringLiteral("v"), 0, 0) <= 1) {
			body.setArray(db->getAccountList());
		} else {
			body.setObject({
				{QStringLiteral("accounts"), db->getAccountList()},
				{QStringLiteral("_locked"), sectionLocked},
			});
		}
		return JsonApiResult{JsonApiResult::Ok, body};

	} else if(method == JsonApiMethod::Create) {
		QString username = request[QStringLiteral("username")].toString();
		QString password = request[QStringLiteral("password")].toString();
		bool locked = request[QStringLiteral("locked")].toBool();
		QString flags = request[QStringLiteral("flags")].toString();
		if(username.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Username required"));
		}
		if(password.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Password required"));
		}
		QJsonObject o =
			db->addAccount(username, password, locked, flags.split(','));
		if(o.isEmpty()) {
			return JsonApiErrorResult(
				JsonApiResult::BadRequest, QStringLiteral("Error"));
		}
		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(o)};

	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult MultiServer::logJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	if(method == JsonApiMethod::Get) {
		ServerLogQuery q = m_config->logger()->query();
		q.page(parseRequestInt(request, QStringLiteral("page"), 0, 0), 100);

		if(request.contains(QStringLiteral("session"))) {
			q.session(request.value(QStringLiteral("session")).toString());
		}

		if(request.contains(QStringLiteral("user"))) {
			q.user(request.value(QStringLiteral("user")).toString());
		}

		if(request.contains(QStringLiteral("contains"))) {
			q.messageContains(
				request.value(QStringLiteral("contains")).toString());
		}

		if(request.contains(QStringLiteral("after"))) {
			QDateTime after = QDateTime::fromString(
				request.value(QStringLiteral("after")).toString(), Qt::ISODate);
			if(after.isValid()) {
				q.after(after);
			} else {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Invalid timestamp"));
			}
		}

		QJsonArray out;
		for(const Log &log : q.omitSensitive(false).get()) {
			out.append(log.toJson());
		}

		return JsonApiResult{JsonApiResult::Ok, QJsonDocument(out)};
	} else if(method == JsonApiMethod::Create) {
		Log entry;

		{
			bool levelOk;
			int level = QMetaEnum::fromType<Log::Level>().keyToValue(
				request.value(QStringLiteral("level"))
					.toString()
					.toUtf8()
					.constData(),
				&levelOk);
			if(!levelOk) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest, QStringLiteral("Invalid level"));
			}

			bool topicOk;
			int topic = QMetaEnum::fromType<Log::Topic>().keyToValue(
				request.value(QStringLiteral("topic"))
					.toString()
					.toUtf8()
					.constData(),
				&topicOk);
			if(!topicOk) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest, QStringLiteral("Invalid topic"));
			}

			entry.about(Log::Level(level), Log::Topic(topic));
		}

		{
			QString session =
				request.value(QStringLiteral("session")).toString();
			if(!session.isEmpty()) {
				if(Ulid(session).isNull()) {
					return JsonApiErrorResult(
						JsonApiResult::BadRequest,
						QStringLiteral("Invalid session"));
				}
				entry.session(session);
			}
		}

		if(request.contains(QStringLiteral("user"))) {
			QJsonObject user = request.value(QStringLiteral("user")).toObject();
			entry.user(
				qBound(0, user.value(QStringLiteral("id")).toInt(), 255),
				QHostAddress(user.value(QStringLiteral("ip")).toString()),
				user.value(QStringLiteral("name")).toString());
		}

		{
			// External logs must start with a source identifier in brackets,
			// followed by whitespace and then some non-whitespace message.
			static QRegularExpression messageRe(
				QStringLiteral("\\A\\[[a-zA-Z0-9_\\.\\-]+\\]\\s+\\S"));
			QString message =
				request.value(QStringLiteral("message")).toString();
			if(!messageRe.match(message).hasMatch()) {
				return JsonApiErrorResult(
					JsonApiResult::BadRequest,
					QStringLiteral("Invalid message"));
			}
			entry.message(message);
		}

		m_config->logger()->logMessage(entry);

		return JsonApiResult{
			JsonApiResult::Ok, QJsonDocument(QJsonObject{
								   {QStringLiteral("_locked"), sectionLocked},
								   {QStringLiteral("entry"), entry.toJson()},
							   })};
	} else {
		return JsonApiBadMethod();
	}
}

JsonApiResult MultiServer::extbansJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request,
	bool sectionLocked)
{
	int pathLength = path.length();
	if(pathLength == 0) {
		if(method == JsonApiMethod::Get || method == JsonApiMethod::Delete) {
			if(method == JsonApiMethod::Delete) {
				m_config->setConfigString(config::ExtBansCacheUrl, QString());
				m_config->setConfigString(config::ExtBansCacheKey, QString());
				m_config->setExternalBans({});
			}
			QJsonObject out = {
				{QStringLiteral("config"),
				 QJsonObject{
					 {config::ExtBansUrl.name,
					  m_config->getConfigString(config::ExtBansUrl)},
					 {config::ExtBansCheckInterval.name,
					  m_config->getConfigTime(config::ExtBansCheckInterval)},
					 {config::ExtBansCacheUrl.name,
					  m_config->getConfigString(config::ExtBansCacheUrl)},
					 {config::ExtBansCacheKey.name,
					  m_config->getConfigString(config::ExtBansCacheKey)},
				 }},
				{QStringLiteral("bans"), m_config->getExternalBans()},
				{QStringLiteral("status"), m_extBans->status()},
				{QStringLiteral("_locked"), sectionLocked},
			};
			return JsonApiResult{JsonApiResult::Ok, QJsonDocument(out)};
		} else {
			return JsonApiBadMethod();
		}

	} else if(pathLength == 1) {
		if(path[0] == QStringLiteral("refresh")) {
			if(method == JsonApiMethod::Create) {
				JsonApiResult::Status status;
				QString msg;
				switch(m_extBans->refreshNow()) {
				case ExtBans::RefreshResult::Ok:
					status = JsonApiResult::Ok;
					msg = QStringLiteral("refresh triggered");
					break;
				case ExtBans::RefreshResult::AlreadyInProgress:
					status = JsonApiResult::BadRequest;
					msg = QStringLiteral("refresh already in progress");
					break;
				case ExtBans::RefreshResult::NotActive:
					status = JsonApiResult::BadRequest;
					msg = QStringLiteral("external bans not active");
					break;
				default:
					status = JsonApiResult::InternalError;
					msg = QStringLiteral("internal error");
					break;
				}
				return JsonApiResult{
					status,
					QJsonDocument(QJsonObject{{QStringLiteral("msg"), msg}})};
			} else {
				return JsonApiBadMethod();
			}
		}

		bool ok;
		int id = path[0].toInt(&ok);
		if(ok) {
			if(method == JsonApiMethod::Update) {
				const QString enabledKey = QStringLiteral("enabled");
				if(request.contains(enabledKey)) {
					bool enabled = request[enabledKey].toBool();
					if(m_config->setExternalBanEnabled(id, enabled)) {
						return JsonApiResult{JsonApiResult::Ok, {}};
					} else {
						return JsonApiErrorResult(
							JsonApiResult::NotFound,
							QStringLiteral(
								"External ipban with id '%1' not found")
								.arg(id));
					}
				} else {
					return JsonApiErrorResult(
						JsonApiResult::BadRequest,
						QStringLiteral("Missing 'enabled' in request"));
				}
			} else {
				return JsonApiBadMethod();
			}
		}

		return JsonApiNotFound();

	} else {
		return JsonApiNotFound();
	}
}

JsonApiResult MultiServer::locksJsonApi(
	JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty()) {
		return JsonApiNotFound();
	}

	bool supported = m_config->supportsAdminSectionLocks();
	QSet<QString> availableSections = {
		QStringLiteral("accounts"), QStringLiteral("bans"),
		QStringLiteral("extbans"),	QStringLiteral("listserverwhitelist"),
		QStringLiteral("server"),	QStringLiteral("sessions"),
	};

	if(method == JsonApiMethod::Update && supported) {
		QString password = request.value(QStringLiteral("password")).toString();
		if(m_config->checkAdminSectionLockPassword(password)) {
			QSet<QString> sectionsToLock;
			for(const QJsonValue sectionValue :
				request.value(QStringLiteral("sections")).toArray()) {
				QString section = sectionValue.toString();
				if(availableSections.contains(section)) {
					sectionsToLock.insert(section);
				}
			}
			bool unlock = sectionsToLock.isEmpty();
			if(m_config->setAdminSectionsLocked(
				   sectionsToLock, unlock ? QString() : password)) {
				QString message;
				if(unlock) {
					message = QStringLiteral("Admin sections unlocked");
				} else {
					QStringList sortedSections(
						sectionsToLock.constBegin(), sectionsToLock.constEnd());
					sortedSections.sort();
					message =
						QStringLiteral(
							"Admin sections locked (with%1 password): %2")
							.arg(
								password.isEmpty() ? QStringLiteral("out")
												   : QString(),
								sortedSections.join(QStringLiteral(", ")));
				}
				m_sessions->config()->logger()->logMessage(
					Log()
						.about(Log::Level::Info, Log::Topic::Status)
						.message(message));
			} else {
				return JsonApiErrorResult(
					JsonApiResult::InternalError,
					QStringLiteral("Database error"));
			}
		} else {
			m_sessions->config()->logger()->logMessage(
				Log()
					.about(Log::Level::Error, Log::Topic::Status)
					.message(QStringLiteral(
						"Incorrect admin section unlock password entered")));
			return JsonApiErrorResult(
				JsonApiResult::Forbidden, QStringLiteral("Incorrect password"));
		}
	} else if(method != JsonApiMethod::Get) {
		return JsonApiBadMethod();
	}

	QJsonObject lockedSections;
	for(const QString &section : availableSections) {
		lockedSections.insert(section, m_config->isAdminSectionLocked(section));
	}
	return JsonApiResult{
		JsonApiResult::Ok,
		QJsonDocument(QJsonObject{
			{QStringLiteral("supported"), supported},
			{QStringLiteral("sections"), lockedSections},
			{QStringLiteral("password"),
			 !m_config->checkAdminSectionLockPassword(QString())},
		})};
}

}
