/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "../libthicksrv/thickserver.h"
#include "../libserver/inmemoryconfig.h"
#include "../libserver/sslserver.h"

#include <QGuiApplication>
#include <QCommandLineParser>
#include <QSslSocket>

static void printVersion()
{
	printf("drawpile-srv " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
	printf("SSL library version: %s (%lu)\n", QSslSocket::sslLibraryVersionString().toLocal8Bit().constData(), QSslSocket::sslLibraryVersionNumber());
}


int main(int argc, char *argv[])
{
	QGuiApplication app(argc, argv);

	QGuiApplication::setOrganizationName("drawpile");
	QGuiApplication::setOrganizationDomain("drawpile.net");
	QGuiApplication::setApplicationName("drawpile-thicksrv");
	QGuiApplication::setApplicationVersion(DRAWPILE_VERSION);

	QCommandLineParser parser;

	parser.setApplicationDescription("Experimental smart server for Drawpile");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

	// --port, -p <port>
	QCommandLineOption portOption(QStringList() << "port" << "p", "Listening port", "port", QString::number(DRAWPILE_PROTO_DEFAULT_PORT));
	parser.addOption(portOption);

	// --listen, -l <address>
	QCommandLineOption listenOption(QStringList() << "listen" << "l", "Listening address", "address");
	parser.addOption(listenOption);

	// --local-host
	QCommandLineOption localAddr("local-host", "This server's hostname for session announcement", "hostname");
	parser.addOption(localAddr);

	// --announce-port <port>
	QCommandLineOption announcePortOption(QStringList() << "announce-port", "Port number to announce (set if forwarding from different port)", "port");
	parser.addOption(announcePortOption);

	// --extauth <url>
#ifdef HAVE_LIBSODIUM
	QCommandLineOption extAuthOption(QStringList() << "extauth", "Extauth server URL", "url");
	parser.addOption(extAuthOption);
#endif

	// --ssl-cert <certificate file>
	QCommandLineOption sslCertOption("ssl-cert", "SSL certificate file", "certificate");
	parser.addOption(sslCertOption);

	// --ssl-key <key file>
	QCommandLineOption sslKeyOption("ssl-key", "SSL key file", "key");
	parser.addOption(sslKeyOption);

	QCommandLineOption recordOption("record", "Record sessions", "path");
	parser.addOption(recordOption);

	// --report-url <url>
	QCommandLineOption reportUrlOption(QStringList() << "report-url", "Abuse report handler URL", "url");
	parser.addOption(reportUrlOption);

	// --template, -t <file>
	QCommandLineOption templateOption(QStringList() << "template" << "t", "Session template", "file");
	parser.addOption(templateOption);

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		::exit(0);
	}

	// TODO server config
	server::ServerConfig *serverconfig = new server::InMemoryConfig;

	// Set internal server config
	server::InternalConfig icfg;
	icfg.localHostname = parser.value(localAddr);
#ifdef HAVE_LIBSODIUM
	icfg.extAuthUrl = parser.value(extAuthOption);
#endif
	icfg.reportUrl = parser.value(reportUrlOption);

	if(parser.isSet(announcePortOption)) {
		bool ok;
		icfg.announcePort = parser.value(announcePortOption).toInt(&ok);
		if(!ok || icfg.announcePort>0xffff) {
			qCritical("Invalid port %s", qPrintable(parser.value(announcePortOption)));
			return 1;
		}
	}

	serverconfig->setInternalConfig(icfg);

	auto *server = new server::ThickServer(serverconfig);
	serverconfig->setParent(server);

	server->connect(server, &server::ThickServer::serverStopped, &app, &QGuiApplication::quit);

	int port;
	{
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			qCritical("Invalid port %s", qPrintable(parser.value(portOption)));
			return 1;
		}
	}

	QHostAddress address = QHostAddress::Any;
	{
		QString av = parser.value(listenOption);
		if(!av.isEmpty()) {
			if(!address.setAddress(av)) {
				qCritical("Invalid listening address %s", qPrintable(av));
				return 1;
			}
		}
	}

	{
		QString sslCert = parser.value(sslCertOption);
		QString sslKey = parser.value(sslKeyOption);
		if(!sslCert.isEmpty() && !sslKey.isEmpty()) {
			server->setSslCertFile(sslCert, sslKey);
			server::SslServer::requireForwardSecrecy();
		}
	}

	{
		QString recordingPath = parser.value(recordOption);
		if(!recordingPath.isEmpty()) {
			server->setRecordingPath(recordingPath);
		}
	}

	// TODO catch signals

	// Start
	if(!server->start(port, address))
		return 1;

	server->setAutoStop(true);

	return app.exec();
}

