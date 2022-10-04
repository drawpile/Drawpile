/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2019 Calle Laakkonen

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

#include "multiserver.h"
#include "initsys.h"
#include "database.h"
#include "templatefiles.h"

#include "../libserver/session.h"
#include "../libserver/sessionserver.h"
#include "../libserver/thinserverclient.h"
#include "../libserver/serverconfig.h"
#include "../libserver/serverlog.h"
#include "../libserver/sslserver.h"
#include "../libshared/util/whatismyip.h"

#include <QTcpSocket>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>

namespace server {

MultiServer::MultiServer(ServerConfig *config, QObject *parent)
	: QObject(parent),
	m_config(config),
	m_server(nullptr),
	m_state(STOPPED),
	m_autoStop(false),
	m_port(0)
{
	m_sessions = new SessionServer(config, this);
	m_started = QDateTime::currentDateTimeUtc();

	connect(m_sessions, &SessionServer::sessionCreated, this, &MultiServer::assignRecording);
	connect(m_sessions, &SessionServer::sessionEnded, this, &MultiServer::tryAutoStop);
	connect(m_sessions, &SessionServer::userCountChanged, [this](int users) {
		printStatusUpdate();
		emit userCountChanged(users);
		// The server will be fully stopped after all users have disconnected
		if(users == 0) {
			if(m_state == STOPPING)
				stop();
			else
				tryAutoStop();
		}
	});
}

#ifndef NDEBUG
void MultiServer::setRandomLag(uint lag)
{
	m_sessions->setRandomLag(lag);
}
#endif

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
	if(!m_sslCertFile.isEmpty() && !m_sslKeyFile.isEmpty()) {
		SslServer *server = new SslServer(m_sslCertFile, m_sslKeyFile, this);
		if(!server->isValidCert()) {
			emit serverStartError("Couldn't load TLS certificate");
			return false;
		}
		m_server = server;

	} else {
		m_server = new QTcpServer(this);
	}

	connect(m_server, &QTcpServer::newConnection, this, &MultiServer::newClient);

	return true;
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param address listening address
 * @return true on success
 */
bool MultiServer::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(m_state == STOPPED);
	m_state = RUNNING;
	if(!createServer()) {
		delete m_server;
		m_server = nullptr;
		m_state = STOPPED;
		return false;
	}

	if(!m_server->listen(address, port)) {
		emit serverStartError(m_server->errorString());
		m_sessions->config()->logger()->logMessage(Log().about(Log::Level::Error, Log::Topic::Status).message(m_server->errorString()));
		delete m_server;
		m_server = nullptr;
		m_state = STOPPED;
		return false;
	}

	m_port = m_server->serverPort();

	InternalConfig icfg = m_config->internalConfig();
	icfg.realPort = m_port;
	m_config->setInternalConfig(icfg);

	emit serverStarted();
	m_sessions->config()->logger()->logMessage(Log().about(Log::Level::Info, Log::Topic::Status)
		.message(QString("Started listening on port %1 at address %2").arg(port).arg(address.toString())));
	return true;
}

/**
 * @brief Start listening on the given file descriptor
 * @param fd
 * @return true on success
 */
bool MultiServer::startFd(int fd)
{
	Q_ASSERT(m_state == STOPPED);
	m_state = RUNNING;
	if(!createServer())
		return false;

	if(!m_server->setSocketDescriptor(fd)) {
		m_sessions->config()->logger()->logMessage(Log().about(Log::Level::Error, Log::Topic::Status).message("Couldn't set server socket descriptor!"));
		delete m_server;
		m_server = nullptr;
		m_state = STOPPED;
		return false;
	}

	m_port = m_server->serverPort();

	m_sessions->config()->logger()->logMessage(Log().about(Log::Level::Info, Log::Topic::Status)
		.message(QString("Started listening on passed socket")));

	return true;
}

/**
 * @brief Assign a recording file name to a new session
 *
 * The name is generated by replacing placeholders in the file name pattern.
 * If a file with the same name exists, a number is inserted just before the suffix.
 *
 * If the file name pattern points to a directory, the default pattern "%d %t session %i.dprec"
 * will be used.
 *
 * The following placeholders are supported:
 *
 *  ~/ - user's home directory (at the start of the pattern)
 *  %d - the current date (YYYY-MM-DD)
 *  %h - the current time (HH.MM.SS)
 *  %i - session ID
 *  %a - session alias (or ID if not assigned)
 *
 * @param session
 */
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
		filename = QFileInfo(QDir(filename), "%d %t session %i.dprec").absoluteFilePath();
	}

	// Expand placeholders
	QDateTime now = QDateTime::currentDateTime();
	filename.replace("%d", now.toString("yyyy-MM-dd"));
	filename.replace("%t", now.toString("HH.mm.ss"));
	filename.replace("%i", session->id());
	filename.replace("%a", session->aliasOrId());

	fi = filename;

	if(!fi.absoluteDir().mkpath(".")) {
		qWarning("Recording directory \"%s\" does not exist and cannot be created!", qPrintable(fi.absolutePath()));
	} else {
		session->setRecordingFile(fi.absoluteFilePath());
	}
}

/**
 * @brief Accept or reject new client connection
 */
void MultiServer::newClient()
{
	QTcpSocket *socket = m_server->nextPendingConnection();

	m_sessions->config()->logger()->logMessage(Log().about(Log::Level::Info, Log::Topic::Status)
		.user(0, socket->peerAddress(), QString())
		.message(QStringLiteral("New client connected")));

	auto *client = new ThinServerClient(socket, m_sessions->config()->logger());

	if(m_config->isAddressBanned(socket->peerAddress())) {
		client->log(Log().about(Log::Level::Warn, Log::Topic::Kick)
			.user(0, socket->peerAddress(), QString())
			.message("Kicking banned user straight away"));

		client->disconnectClient(Client::DisconnectionReason::Error, "BANNED");

	} else {
		m_sessions->addClient(client);
		printStatusUpdate();
	}
}


void MultiServer::printStatusUpdate()
{
	initsys::notifyStatus(QString("%1 users and %2 sessions")
		.arg(m_sessions->totalUsers())
		.arg(m_sessions->sessionCount())
	);
}

/**
 * @brief Stop the server if vacant (and autostop is enabled)
 */
void MultiServer::tryAutoStop()
{
	if(m_state == RUNNING && m_autoStop && m_sessions->sessionCount() == 0 && m_sessions->totalUsers() == 0) {
		m_sessions->config()->logger()->logMessage(Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message("Autostopping due to lack of sessions."));
		stop();
	}
}

/**
 * Disconnect all clients and stop listening.
 */
void MultiServer::stop() {
	if(m_state == RUNNING) {
		m_sessions->config()->logger()->logMessage(Log()
			.about(Log::Level::Info, Log::Topic::Status)
			.message(QString("Stopping server and kicking out %1 users...")
					 .arg(m_sessions->totalUsers())
			));

		m_state = STOPPING;
		m_server->close();
		m_port = 0;

		m_sessions->stopAll();
	}

	if(m_state == STOPPING) {
		if(m_sessions->totalUsers() == 0) {
			m_state = STOPPED;
			delete m_server;
			m_server = nullptr;
			m_sessions->config()->logger()->logMessage(Log()
				.about(Log::Level::Info, Log::Topic::Status)
				.message("Server stopped."));
			emit serverStopped();
		}
	}
}

JsonApiResult MultiServer::callJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	QString head;
	QStringList tail;
	std::tie(head, tail) = popApiPath(path);

	if(head == "server")
		return serverJsonApi(method, tail, request);
	else if(head == "status")
		return statusJsonApi(method, tail, request);
	else if(head == "sessions")
		return m_sessions->callSessionJsonApi(method, tail, request);
	else if(head == "users")
		return m_sessions->callUserJsonApi(method, tail, request);
	else if(head == "banlist")
		return banlistJsonApi(method, tail, request);
	else if(head == "listserverwhitelist")
		return listserverWhitelistJsonApi(method, tail, request);
	else if(head == "accounts")
		return accountsJsonApi(method, tail, request);
	else if(head == "log")
		return logJsonApi(method, tail, request);

	return JsonApiNotFound();
}

void MultiServer::callJsonApiAsync(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	JsonApiResult result = callJsonApi(method, path, request);
	emit jsonApiResult(requestId, result);
}

/**
 * @brief Serverwide settings
 *
 * @param method
 * @param path
 * @param request
 * @return
 */
JsonApiResult MultiServer::serverJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method != JsonApiMethod::Get && method != JsonApiMethod::Update)
		return JsonApiBadMethod();

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
		config::PrivateUserList,
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
#endif
		config::LogPurgeDays,
		config::AllowCustomAvatars,
		config::AbuseReport,
		config::ReportToken,
		config::ForceNsfm,
	};
	const int settingCount = sizeof(settings) / sizeof(settings[0]);

	if(method==JsonApiMethod::Update) {
		for(int i=0;i<settingCount;++i) {
			if(request.contains(settings[i].name)) {
				m_config->setConfigString(settings[i], request[settings[i].name].toVariant().toString());
			}
		}
	}

	QJsonObject result;
	for(int i=0;i<settingCount;++i) {
		result[settings[i].name] = QJsonValue::fromVariant(m_config->getConfigVariant(settings[i]));
	}

	// Hide values for disabled features
	if(!m_config->internalConfig().reportUrl.isValid())
		result.remove(config::AbuseReport.name);

	if(!m_config->internalConfig().extAuthUrl.isValid())
		result.remove(config::UseExtAuth.name);

	return JsonApiResult { JsonApiResult::Ok, QJsonDocument(result) };
}

/**
 * @brief Read only view of server status
 *
 * @param method
 * @param path
 * @param request
 * @return
 */
JsonApiResult MultiServer::statusJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	Q_UNUSED(request);

	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method != JsonApiMethod::Get)
		return JsonApiBadMethod();

	QJsonObject result;
	result["started"] = m_started.toString("yyyy-MM-dd HH:mm:ss");
	result["sessions"] = m_sessions->sessionCount();
	result["maxSessions"] = m_config->getConfigInt(config::SessionCountLimit);
	result["users"] = m_sessions->totalUsers();
	QString localhost = m_config->internalConfig().localHostname;
	if(localhost.isEmpty())
		localhost = WhatIsMyIp::guessLocalAddress();
	result["ext_host"] = localhost;
	result["ext_port"] = m_config->internalConfig().getAnnouncePort();

	return JsonApiResult { JsonApiResult::Ok, QJsonDocument(result) };
}

/**
 * @brief View and modify the serverwide banlist
 *
 * @param method
 * @param path
 * @param request
 * @return
 */
JsonApiResult MultiServer::banlistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	// Database is needed to manipulate the banlist
	Database *db = qobject_cast<Database*>(m_config);
	if(!db)
		return JsonApiNotFound();

	if(path.size()==1) {
		if(method != JsonApiMethod::Delete)
			return JsonApiBadMethod();
		if(db->deleteBan(path.at(0).toInt())) {
			QJsonObject body;
			body["status"] = "ok";
			body["deleted"] = path.at(0).toInt();
			return JsonApiResult {JsonApiResult::Ok, QJsonDocument(body)};
		} else
			return JsonApiNotFound();
	}

	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Get) {
		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(db->getBanlist()) };

	} else if(method == JsonApiMethod::Create) {
		QHostAddress ip { request["ip"].toString() };
		if(ip.isNull())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Valid IP address required");
		int subnet = request["subnet"].toInt();
		QDateTime expiration = QDateTime::fromString(request["expires"].toString(), "yyyy-MM-dd HH:mm:ss");
		if(expiration.isNull())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Valid expiration time required");
		QString comment = request["comment"].toString();

		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(db->addBan(ip, subnet, expiration, comment)) };

	} else
		return JsonApiBadMethod();
}

/**
 * @brief View and modify the list server URL whitelist 
 *
 * @param method
 * @param path
 * @param request
 * @return
 */
JsonApiResult MultiServer::listserverWhitelistJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	// Database is needed to manipulate the whitelist
	Database *db = qobject_cast<Database*>(m_config);
	if(!db)
		return JsonApiNotFound();

	if(!path.isEmpty())
		return JsonApiNotFound();

	if(method == JsonApiMethod::Update) {
		QStringList whitelist;
		for(const auto &v : request["whitelist"].toArray()) {
			const QString str = v.toString();
			if(str.isEmpty())
				continue;

			const QRegularExpression re(str);
			if(!re.isValid())
				return JsonApiErrorResult(JsonApiResult::BadRequest, str + ": " + re.errorString());
			whitelist << str;
		}
		if(!request["enabled"].isUndefined())
			db->setConfigBool(config::AnnounceWhiteList, request["enabled"].toBool());
		if(!request["whitelist"].isUndefined())
			db->updateListServerWhitelist(whitelist);
	}

	const QJsonObject o {
		{"enabled", db->getConfigBool(config::AnnounceWhiteList)},
		{"whitelist", QJsonArray::fromStringList(db->listServerWhitelist())}
	};

	return JsonApiResult { JsonApiResult::Ok, QJsonDocument(o) };
}

/**
 * @brief View and modify registered user accounts
 *
 * @param method
 * @param path
 * @param request
 * @return
 */
JsonApiResult MultiServer::accountsJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	// Database is needed to manipulate account list
	Database *db = qobject_cast<Database*>(m_config);
	if(!db)
		return JsonApiNotFound();

	if(path.size()==1) {
		if(method == JsonApiMethod::Update) {
			QJsonObject o = db->updateAccount(path.at(0).toInt(), request);
			if(o.isEmpty())
				return JsonApiNotFound();
			return JsonApiResult {JsonApiResult::Ok, QJsonDocument(o)};

		} else if(method == JsonApiMethod::Delete) {
			if(db->deleteAccount(path.at(0).toInt())) {
				QJsonObject body;
				body["status"] = "ok";
				body["deleted"] = path.at(0).toInt();
				return JsonApiResult {JsonApiResult::Ok, QJsonDocument(body)};
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
		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(db->getAccountList()) };

	} else if(method == JsonApiMethod::Create) {
		QString username = request["username"].toString();
		QString password = request["password"].toString();
		bool locked = request["locked"].toBool();
		QString flags = request["flags"].toString();
		if(username.isEmpty())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Username required");
		if(password.isEmpty())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Password required");

		QJsonObject o = db->addAccount(username, password, locked, flags.split(','));
		if(o.isEmpty())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Error");
		return JsonApiResult { JsonApiResult::Ok, QJsonDocument(o) };

	} else
		return JsonApiBadMethod();
}

JsonApiResult MultiServer::logJsonApi(JsonApiMethod method, const QStringList &path, const QJsonObject &request)
{
	if(!path.isEmpty())
		return JsonApiNotFound();
	if(method != JsonApiMethod::Get)
		return JsonApiBadMethod();

	auto q = m_config->logger()->query();
	q.page(request.value("page").toInt(), 100);

	if(request.contains("session"))
		q.session(request.value("session").toString());

	if(request.contains("after")) {
		QDateTime after = QDateTime::fromString(request.value("after").toString(), Qt::ISODate);
		if(!after.isValid())
			return JsonApiErrorResult(JsonApiResult::BadRequest, "Invalid timestamp");
		q.after(after);
	}

	QJsonArray out;
	for(const Log &log : q.get()) {
		out.append(log.toJson());
	}

	return JsonApiResult { JsonApiResult::Ok, QJsonDocument(out) };
}

}
