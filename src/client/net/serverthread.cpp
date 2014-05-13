/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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

#include "config.h"

#include "net/serverthread.h"

#include "builtinserver.h"
#include "../shared/util/logger.h"

namespace net {

ServerThread::ServerThread(QObject *parent)
	: QThread(parent), _deleteonexit(false), _port(DRAWPILE_PROTO_DEFAULT_PORT)
{
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
	server::BuiltinServer server;
#ifdef NDEBUG
	logger::setLogLevel(logger::LOG_WARNING);
#else
	logger::setLogLevel(logger::LOG_DEBUG);
#endif

	connect(&server, SIGNAL(serverStopped()), this, SLOT(quit()));

	logger::info() << "Starting server";
	if(!server.start(_port)) {
		logger::error() << "Couldn't start server!";
		_port = 0;
		_starter.wakeOne();
		return;
	}

	_port = server.port();
	_starter.wakeOne();

	exec();

	logger::info() << "server thread exiting.";
	if(_deleteonexit)
		deleteLater();
}

}
