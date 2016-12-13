/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#include "sslserver.h"
#include "userfile.h"
#include "announcementwhitelist.h"
#include "banlist.h"

#include "../shared/server/session.h"
#include "../shared/server/sessionserver.h"
#include "../shared/server/client.h"

#include "../shared/util/announcementapi.h"

#include <QTcpSocket>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

namespace server {

MultiServer::MultiServer(QObject *parent)
	: QObject(parent),
	_server(nullptr),
	_banlist(nullptr),
	_state(NOT_STARTED),
	_autoStop(false), m_splitRecording(false)
{
	_sessions = new SessionServer(this);

	connect(_sessions, SIGNAL(sessionCreated(Session*)), this, SLOT(assignRecording(Session*)));
	connect(_sessions, SIGNAL(sessionEnded(QString)), this, SLOT(tryAutoStop()));
	connect(_sessions, SIGNAL(userLoggedIn()), this, SLOT(printStatusUpdate()));
	connect(_sessions, &SessionServer::userDisconnected, [this]() {
		printStatusUpdate();
		// The server will be fully stopped after all users have disconnected
		if(_state == STOPPING)
			stop();
		else
			tryAutoStop();
	});
}

void MultiServer::setServerTitle(const QString &title)
{
	_sessions->setTitle(title);
}

void MultiServer::setWelcomeMessage(const QString &message)
{
	_sessions->setWelcomeMessage(message);
}

void MultiServer::setHistoryLimit(uint limit)
{
	_sessions->setHistoryLimit(limit);
}

void MultiServer::setMustSecure(bool secure)
{
	_sessions->setMustSecure(secure);
}

void MultiServer::setHostPassword(const QString &password)
{
	_sessions->setHostPassword(password);
}

/**
 * @brief Set the maximum number of sessions
 *
 * Setting this to 1 disables the "multisession" capability flag. Clients
 * will not display the session selector dialog in that case, but will automatically
 * connect to the only session.
 * @param limit
 */
void MultiServer::setSessionLimit(int limit)
{
	_sessions->setSessionLimit(limit);
}

/**
 * @brief Enable or disable persistent sessions
 * @param persistent
 */
void MultiServer::setPersistentSessions(bool persistent)
{
	_sessions->setAllowPersistentSessions(persistent);
}

/**
 * @brief Set persistent session expiration time
 *
 * Setting this to 0 means sessions will not expire.
 * @param seconds
 */
void MultiServer::setExpirationTime(uint seconds)
{
	_sessions->setExpirationTime(seconds);
}

void MultiServer::setConnectionTimeout(int timeout)
{
	_sessions->setConnectionTimeout(timeout);
}

#ifndef NDEBUG
void MultiServer::setRandomLag(uint lag)
{
	_sessions->setRandomLag(lag);
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
	_autoStop = autostop;
}

bool MultiServer::setUserFile(const QString &path)
{
	UserFile *userfile = new UserFile(_sessions);
	if(!userfile->setFile(path))
		return false;

	_sessions->setIdentityManager(userfile);
	return true;
}

void MultiServer::setAnnounceWhitelist(const QString &path)
{
	logger::info() << "Using announcement whitelist file" << path;
	AnnouncementWhitelist *wl = new AnnouncementWhitelist(this);
	wl->setWhitelistFile(path);
	_sessions->announcementApiClient()->setWhitelist(std::bind(&AnnouncementWhitelist::isWhitelisted, wl, std::placeholders::_1));
}

void MultiServer::setAnnounceLocalAddr(const QString &addr)
{
	_sessions->announcementApiClient()->setLocalAddress(addr);
}

void MultiServer::setBanlist(const QString &path)
{
	logger::info() << "Using IP ban list" << path;
	_banlist = new BanList(this);
	_banlist->setPath(path);
}

void MultiServer::setPrivateUserList(bool p)
{
	_sessions->setPrivateUserList(p);
}

void MultiServer::setAllowGuests(bool allow)
{
	if(!_sessions->identityManager()) {
		if(!allow)
			logger::warning() << "Cannot disable guest access: no user file selected!";
		return;
	}
	_sessions->identityManager()->setAuthorizedOnly(!allow);
}

bool MultiServer::createServer()
{
	if(!_sslCertFile.isEmpty() && !_sslKeyFile.isEmpty()) {
		SslServer *server = new SslServer(_sslCertFile, _sslKeyFile, this);
		if(!server->isValidCert())
			return false;
		_server = server;

	} else {
		_server = new QTcpServer(this);
	}

	connect(_server, SIGNAL(newConnection()), this, SLOT(newClient()));

	return true;
}

/**
 * @brief Start listening on the specified address.
 * @param port the port to listen on
 * @param address listening address
 * @return true on success
 */
bool MultiServer::start(quint16 port, const QHostAddress& address) {
	Q_ASSERT(_state == NOT_STARTED);
	_state = RUNNING;
	if(!createServer())
		return false;

	if(!_server->listen(address, port)) {
		logger::error() << _server->errorString();
		delete _server;
		_server = 0;
		_state = NOT_STARTED;
		return false;
	}

	logger::info() << "Started listening on port" << port << "at address" << address.toString();
	return true;
}

/**
 * @brief Start listening on the given file descriptor
 * @param fd
 * @return true on success
 */
bool MultiServer::startFd(int fd)
{
	Q_ASSERT(_state == NOT_STARTED);
	_state = RUNNING;
	if(!createServer())
		return false;

	if(!_server->setSocketDescriptor(fd)) {
		logger::error() << "Couldn't set server socket descriptor!";
		delete _server;
		_server = 0;
		_state = NOT_STARTED;
		return false;
	}

	logger::info() << "Started listening on passed socket";
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
 *
 * @param session
 */
void MultiServer::assignRecording(Session *session)
{
	if(_recordingFile.isEmpty())
		return;

	QString filename = _recordingFile;

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
	filename.replace("%i", sessionIdString(session->id()));

	fi = filename;

	session->setRecordingFile(fi.absoluteFilePath(), m_splitRecording);
}

/**
 * @brief Accept or reject new client connection
 */
void MultiServer::newClient()
{
	QTcpSocket *socket = _server->nextPendingConnection();

	logger::info() << "Accepted new client from address" << socket->peerAddress();

	auto *client = new Client(socket);
	_sessions->addClient(client);

	if(_banlist) {
		if(_banlist->isBanned(socket->peerAddress())) {
			logger::info() << "Kicking banned client from address" << socket->peerAddress() << "straight away";
			client->disconnectKick("BANNED");
		}
	}

	printStatusUpdate();
}


void MultiServer::printStatusUpdate()
{
	initsys::notifyStatus(QString("%1 users and %2 sessions")
		.arg(_sessions->totalUsers())
		.arg(_sessions->sessionCount())
	);
}

/**
 * @brief Stop the server if vacant (and autostop is enabled)
 */
void MultiServer::tryAutoStop()
{
	if(_state == RUNNING && _autoStop && _sessions->sessionCount() == 0 && _sessions->totalUsers() == 0) {
		logger::info() << "Autostopping due to lack of sessions";
		stop();
	}
}

/**
 * Disconnect all clients and stop listening.
 */
void MultiServer::stop() {
	if(_state == RUNNING) {
		logger::info() << "Stopping server and kicking out" << _sessions->totalUsers() << "users...";
		_state = STOPPING;
		_server->close();

		_sessions->stopAll();
	}

	if(_state == STOPPING) {
		if(_sessions->totalUsers() == 0) {
			_state = STOPPED;
			logger::info() << "Server stopped.";
			emit serverStopped();
		}
	}
}

}
