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

#include "../shared/server/server.h"

using server::Server;

int main(int argc, char *argv[]) {
	QCoreApplication app(argc, argv);

	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Parse command line arguments
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

	// --limit
	QCommandLineOption limitOption("limit", "Limit history size", "size (Mb)");
	parser.addOption(limitOption);

	// --record, -r
	QCommandLineOption recordOption(QStringList() << "record" << "r", "Record session", "filename");
	parser.addOption(recordOption);

	// Parse
	parser.process(app);

	// Initialize the server
	Server *server = new Server;

	server->connect(server, SIGNAL(serverStopped()), &app, SLOT(quit()));

	SharedLogger logger(new ConsoleLogger);
	if(parser.isSet(verboseOption))
		logger->setLogLevel(Logger::LOG_DEBUG);
	else
		logger->setLogLevel(Logger::LOG_WARNING);
	server->setLogger(logger);

	int port = DRAWPILE_PROTO_DEFAULT_PORT;
	QHostAddress address = QHostAddress::Any;

	if(parser.isSet(portOption)) {
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			logger->logError("Invalid port");
			return 1;
		}
	}

	if(parser.isSet(listenOption)) {
		if(!address.setAddress(parser.value(listenOption))) {
			logger->logError("Invalid listening address");
			return 1;
		}
	}

	if(parser.isSet(limitOption)) {
		bool ok;
		float limit = parser.value(limitOption).toFloat(&ok);
		if(!ok) {
			logger->logError("Invalid history limit size");
			return 1;
		}
		uint limitbytes = limit * 1024 * 1024;
		server->setHistorylimit(limitbytes);
	}

	if(parser.isSet(recordOption))
		server->setRecordingFile(parser.value(recordOption));

	// Dedicated server shouldn't shut down just because there are no users
	server->setPersistent(true);

	// Start
	if(!server->start(port, false, address))
		return 1;

	return app.exec();
}

