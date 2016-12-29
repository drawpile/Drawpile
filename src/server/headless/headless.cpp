/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2016 Calle Laakkonen

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

#include "multiserver.h"
#include "initsys.h"
#include "sslserver.h"
#include "database.h"

#include "../shared/util/logger.h"

#include <QCoreApplication>
#include <QStringList>
#include <QSslSocket>
#include <QCommandLineParser>

#ifdef Q_OS_UNIX
#include "unixsignals.h"
#endif

namespace server {
namespace headless {

void printVersion()
{
	printf("drawpile-srv " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
	printf("SSL library version: %s (%lu)\n", QSslSocket::sslLibraryVersionString().toLocal8Bit().constData(), QSslSocket::sslLibraryVersionNumber());
}

bool start() {
	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Standalone server for Drawpile");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --verbose, -V
	QCommandLineOption verboseOption(QStringList() << "verbose" << "V", "Verbose mode");
	parser.addOption(verboseOption);

	// --port, -p <port>
	QCommandLineOption portOption(QStringList() << "port" << "p", "Listening port", "port", QString::number(DRAWPILE_PROTO_DEFAULT_PORT));
	parser.addOption(portOption);

	// --listen, -l <address>
	QCommandLineOption listenOption(QStringList() << "listen" << "l", "Listening address", "address");
	parser.addOption(listenOption);

	// --ssl-cert <certificate file>
	QCommandLineOption sslCertOption("ssl-cert", "SSL certificate file", "certificate");
	parser.addOption(sslCertOption);

	// --ssl-key <key file>
	QCommandLineOption sslKeyOption("ssl-key", "SSL key file", "key");
	parser.addOption(sslKeyOption);

	// --secure, -S
	QCommandLineOption secureOption(QStringList() << "secure" << "S", "Mandatory SSL mode");
	parser.addOption(secureOption);

#ifndef NDEBUG
	QCommandLineOption lagOption("random-lag", "Randomly sleep to simulate lag", "msecs", "0");
	parser.addOption(lagOption);
#endif

	// --database, -d <filename>
	QCommandLineOption dbFileOption(QStringList() << "database" << "d", "Use configuration database", "filename");
	parser.addOption(dbFileOption);

	// Parse
	parser.process(*QCoreApplication::instance());

	if(parser.isSet(versionOption)) {
		printVersion();
		::exit(0);
	}

	// Open server configuration database
	ServerConfig *serverconfig;
	if(parser.isSet(dbFileOption)) {
		auto *db = new Database;
		if(!db->openFile(parser.value(dbFileOption))) {
			logger::error() << "Couldn't open database file" << parser.value(dbFileOption);
			delete db;
			return false;
		}
		serverconfig = db;

	} else {
		// No database file given: settings are non-persistent
		serverconfig = new ServerConfig;
	}

	// Initialize the server
	server::MultiServer *server = new server::MultiServer(serverconfig);
	serverconfig->setParent(server);

	server->connect(server, SIGNAL(serverStopped()), QCoreApplication::instance(), SLOT(quit()));

	if(parser.isSet(verboseOption)) {
#ifdef NDEBUG
		logger::setLogLevel(logger::LOG_INFO);
#else
		logger::setLogLevel(logger::LOG_DEBUG);
#endif
	} else {
		logger::setLogLevel(logger::LOG_WARNING);
	}

	int port;
	{
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			logger::error() << "Invalid port" << parser.value(portOption);
			return false;
		}
	}

	QHostAddress address = QHostAddress::Any;
	{
		QString av = parser.value(listenOption);
		if(!av.isEmpty()) {
			if(!address.setAddress(av)) {
				logger::error() << "Invalid listening address" << av;
				return false;
			}
		}
	}

	{
		QString sslCert = parser.value(sslCertOption);
		QString sslKey = parser.value(sslKeyOption);
		if(!sslCert.isEmpty() && !sslKey.isEmpty()) {
			server->setSslCertFile(sslCert, sslKey);
			server->setMustSecure(parser.isSet(secureOption));
			server::SslServer::requireForwardSecrecy();
		}
	}

#ifndef NDEBUG
	{
		uint lag = parser.value(lagOption).toUInt();
		server->setRandomLag(lag);
	}
#endif

	// Catch signals
#ifdef Q_OS_UNIX
	server->connect(UnixSignals::instance(), SIGNAL(sigInt()), server, SLOT(stop()));
	server->connect(UnixSignals::instance(), SIGNAL(sigTerm()), server, SLOT(stop()));
#endif

	// Start
	{
		QList<int> listenfds = initsys::getListenFds();
		if(listenfds.isEmpty()) {
			// socket activation not used
			if(!server->start(port, address))
				return false;

		} else {
			// listening socket passed to us by the init system
			if(listenfds.size() != 1) {
				logger::error() << "Too many file descriptors received.";
				return false;
			}

			server->setAutoStop(true);

			if(!server->startFd(listenfds[0]))
				return false;

			// TODO start webadmin if two fds were passsed
		}
	}


	initsys::notifyReady();

	return true;
}

}
}
