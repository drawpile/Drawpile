/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#include <QCoreApplication>
#include <QStringList>

#if (QT_VERSION < QT_VERSION_CHECK(5, 2, 0))
#include "qcommandlineparser.h"
#else
#include <QCommandLineParser>
#endif

#include "config.h"

#include "multiserver.h"
#include "initsys.h"
#include "../shared/util/logger.h"

#ifdef Q_OS_UNIX
#include <iostream>
#include <unistd.h>
#include "unixsignals.h"
#endif

int main(int argc, char *argv[]) {
#ifdef Q_OS_UNIX
	// Security check
	if(geteuid() == 0) {
		std::cerr << "This program should not be run as root!\n";
		return 1;
	}
#endif

	QCoreApplication app(argc, argv);

	QCoreApplication::setOrganizationName("DrawPile");
	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	initsys::setInitSysLogger();

	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Standalone server for Drawpile");
	parser.addHelpOption();
	parser.addVersionOption();

	// --verbose, -V
	QCommandLineOption verboseOption(QStringList() << "verbose" << "V", "Verbose mode");
	parser.addOption(verboseOption);

	// --port, -p <port>
	QCommandLineOption portOption(QStringList() << "port" << "p", "Listening port", "port");
	parser.addOption(portOption);

	// --listen, -l <address>
	QCommandLineOption listenOption(QStringList() << "listen" << "l", "Listening address", "address");
	parser.addOption(listenOption);

	// --limit <size>
	QCommandLineOption limitOption("history-limit", "Limit history size", "size (Mb)");
	parser.addOption(limitOption);

	// --record, -r <filename>
	QCommandLineOption recordOption(QStringList() << "record" << "r", "Record session", "filename");
	parser.addOption(recordOption);

	// --host-password <password>
	QCommandLineOption hostPassOption("host-password", "Host password", "password");
	parser.addOption(hostPassOption);

	// --sessions <count>
	QCommandLineOption sessionLimitOption("sessions", "Maximum number of sessions", "count");
	parser.addOption(sessionLimitOption);

	// --persistent, -P
	QCommandLineOption persistentSessionOption(QStringList() << "persistent" << "P", "Enable persistent sessions");
	parser.addOption(persistentSessionOption);

	// Parse
	parser.process(app);

	// Initialize the server
	server::MultiServer *server = new server::MultiServer;

	server->connect(server, SIGNAL(serverStopped()), &app, SLOT(quit()));

	if(parser.isSet(verboseOption)) {
#ifdef NDEBUG
		logger::setLogLevel(logger::LOG_INFO);
#else
		logger::setLogLevel(logger::LOG_DEBUG);
#endif
	} else {
		logger::setLogLevel(logger::LOG_WARNING);
	}

	int port = DRAWPILE_PROTO_DEFAULT_PORT;
	QHostAddress address = QHostAddress::Any;

	if(parser.isSet(portOption)) {
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			logger::error() << "Invalid port";
			return 1;
		}
	}

	if(parser.isSet(listenOption)) {
		if(!address.setAddress(parser.value(listenOption))) {
			logger::error() << "Invalid listening address";
			return 1;
		}
	}

	if(parser.isSet(limitOption)) {
		bool ok;
		float limit = parser.value(limitOption).toFloat(&ok);
		if(!ok) {
			logger::error() << "Invalid history limit size";
			return 1;
		}
		uint limitbytes = limit * 1024 * 1024;
		server->setHistoryLimit(limitbytes);
	}

	if(parser.isSet(recordOption))
		server->setRecordingFile(parser.value(recordOption));

	if(parser.isSet(hostPassOption))
		server->setHostPassword(parser.value(hostPassOption));

	int sessionLimit = 20;
	if(parser.isSet(sessionLimitOption)) {
		bool ok;
		sessionLimit = parser.value(sessionLimitOption).toInt(&ok);
		if(!ok || sessionLimit<1) {
			logger::error() << "Invalid session count limit";
			return 1;
		}
	}
	server->setSessionLimit(sessionLimit);

	server->setPersistentSessions(parser.isSet(persistentSessionOption));

#ifdef Q_OS_UNIX
	// Catch signals
	server->connect(UnixSignals::instance(), SIGNAL(sigInt()), server, SLOT(stop()));
	server->connect(UnixSignals::instance(), SIGNAL(sigTerm()), server, SLOT(stop()));
#endif

	// Start
	{
		QList<int> listenfds = initsys::getListenFds();
		if(listenfds.isEmpty()) {
			// socket activation not used
			if(!server->start(port, address))
				return 1;
		} else {
			// listening socket passed to us by the init system
			if(listenfds.size() != 1) {
				logger::error() << "Too many file descriptors received.";
				return 1;
			}

			server->setAutoStop(true);

			if(!server->startFd(listenfds[0]))
				return 1;
		}
	}


	initsys::notifyReady();

	return app.exec();
}

