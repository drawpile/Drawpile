/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "config.h"

#include "net/serverthread.h"

#include "../shared/server/server.h"
#include "../shared/server/server.h"

namespace net {

ServerThread::ServerThread(QObject *parent) : QThread(parent)
{
	_deleteonexit = false;
    _port = DRAWPILE_PROTO_DEFAULT_PORT;
	_historylimit = 0;
}

int ServerThread::startServer()
{
	_startmutex.lock();

	start();

	_starter.wait(&_startmutex);
	_startmutex.unlock();

	return _port;
}

bool ServerThread::isOnDefaultPort() const
{
	return _port == DRAWPILE_PROTO_DEFAULT_PORT;
}

void ServerThread::run() {
	server::Server server;
	SharedLogger logger(new ConsoleLogger);
#ifdef NDEBUG
	logger->setLogLevel(Logger::LOG_WARNING);
#else
	logger->setLogLevel(Logger::LOG_DEBUG);
#endif
	server.setLogger(logger);

	server.setHistorylimit(_historylimit);

	connect(&server, SIGNAL(lastClientLeft()), this, SLOT(quit()));

	logger->logDebug("Starting server");
    if(!server.start(_port, true)) {
		logger->logError("Couldn't start server");
		_port = 0;
		_starter.wakeOne();
		return;
	}

	_port = server.port();
	_starter.wakeOne();

	exec();

	logger->logDebug("server thread exiting.");
	if(_deleteonexit)
		deleteLater();
}

}
