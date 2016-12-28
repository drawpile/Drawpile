/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#include <QCoreApplication>
#include <QStringList>
#include <QScopedPointer>
#include <QRegularExpression>
#include <QSslSocket>
#include <QCommandLineParser>

#include "config.h"

#include "multiserver.h"
#include "initsys.h"
#include "configfile.h"
#include "sslserver.h"
#include "../shared/util/logger.h"

#include <cstdio>

#ifdef Q_OS_UNIX
#include <unistd.h>
#include "unixsignals.h"
#endif

void printVersion()
{
	printf("drawpile-srv " DRAWPILE_VERSION "\n");
	printf("Protocol version: %d.%d\n", DRAWPILE_PROTO_MAJOR_VERSION, DRAWPILE_PROTO_MINOR_VERSION);
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
	printf("SSL library version: %s (%lu)\n", QSslSocket::sslLibraryVersionString().toLocal8Bit().constData(), QSslSocket::sslLibraryVersionNumber());
}

int main(int argc, char *argv[]) {
#ifdef Q_OS_UNIX
	// Security check
	if(geteuid() == 0) {
		fprintf(stderr, "This program should not be run as root!\n");
		return 1;
	}
#endif

	QCoreApplication app(argc, argv);

	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.sourceforge.net");
	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	initsys::setInitSysLogger();

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

	// --limit <size>
	QCommandLineOption limitOption("history-limit", "Limit history size", "size (Mb)", "0");
	parser.addOption(limitOption);

	// --record, -r <filename>
	QCommandLineOption recordOption(QStringList() << "record" << "r", "Record session", "filename");
	parser.addOption(recordOption);

	// --split-recording
	QCommandLineOption splitRecordingOption(QStringList() << "split-recording", "Start a new recording at every snapshot");
	parser.addOption(splitRecordingOption);

	// --host-password <password>
	QCommandLineOption hostPassOption("host-password", "Host password", "password");
	parser.addOption(hostPassOption);

	// --sessions <count>
	QCommandLineOption sessionLimitOption("sessions", "Maximum number of sessions", "count", "20");
	parser.addOption(sessionLimitOption);

	// --persistent, -P
	QCommandLineOption persistentSessionOption(QStringList() << "persistent" << "P", "Enable persistent sessions");
	parser.addOption(persistentSessionOption);

	// --expire <time> (<time>[prefix], where prefix is d, h, m or s. No prefix defaults to s)
	QCommandLineOption expireOption("expire", "Idle session expiration time", "expiration", "0");
	parser.addOption(expireOption);

	// --title, -t <server title>
	QCommandLineOption serverTitleOption(QStringList() << "title" << "t", "Set server title", "title");
	parser.addOption(serverTitleOption);

	// --welcome-message <message>
	QCommandLineOption serverWelcomeOption(QStringList() << "welcome-message", "Set welcome message", "message");
	parser.addOption(serverWelcomeOption);

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

	// --userfile <path>
	QCommandLineOption userfileOption("userfile", "Use user/password file", "path");
	parser.addOption(userfileOption);

	// --no-guests
	QCommandLineOption noGuestsOption("no-guests", "Users must authenticate to log in");
	parser.addOption(noGuestsOption);

	// --timeout
	QCommandLineOption timeoutOption("timeout", "Connection timeout", "seconds", "60");
	parser.addOption(timeoutOption);

	// --announce-whitelist
	QCommandLineOption announceWhitelist("announce-whitelist", "Session announcement server whitelist", "filename");
	parser.addOption(announceWhitelist);

	// --announce-local-addr
	QCommandLineOption announceLocalAddr("announce-local-addr", "Local address for session announcement", "address");
	parser.addOption(announceLocalAddr);

	// --banlist
	QCommandLineOption banlist("banlist", "IP banlist", "filename");
	parser.addOption(banlist);

	// --private-userlist
	QCommandLineOption privateUserListOption("private-userlist", "Never include user lists in announcements");
	parser.addOption(privateUserListOption);

	// --config, -c <filename>
	QCommandLineOption configFileOption(QStringList() << "config" << "c", "Load configuration file", "filename");
	parser.addOption(configFileOption);

	// Parse
	parser.process(app);

	if(parser.isSet(versionOption)) {
		printVersion();
		return 0;
	}

	// Load configuration file (if set)
	ConfigFile cfgfile(parser.value(configFileOption));

	// Initialize the server
	server::MultiServer *server = new server::MultiServer;

	server->connect(server, SIGNAL(serverStopped()), &app, SLOT(quit()));

	server->setServerTitle(cfgfile.override(parser, serverTitleOption).toString());
	server->setWelcomeMessage(cfgfile.override(parser, serverWelcomeOption).toString());

	if(cfgfile.override(parser, verboseOption).toBool()) {
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
		QVariant pv = cfgfile.override(parser, portOption);
		port = pv.toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			logger::error() << "Invalid port" << pv.toString();
			return 1;
		}
	}

	QHostAddress address = QHostAddress::Any;
	{
		QVariant av = cfgfile.override(parser, listenOption);
		if(!av.isNull()) {
			if(!address.setAddress(av.toString())) {
				logger::error() << "Invalid listening address" << av.toString();
				return 1;
			}
		}
	}

	{
		QVariant lv = cfgfile.override(parser, limitOption);
		bool ok;
		float limit = lv.toFloat(&ok);
		if(!ok || limit<0) {
			logger::error() << "Invalid history limit: " << lv.toString();
			return 1;
		}
		uint limitbytes = limit * 1024 * 1024;
		server->setHistoryLimit(limitbytes);
	}

	{
		QVariant rv = cfgfile.override(parser, recordOption);
		if(!rv.isNull()) {
			server->setRecordingFile(rv.toString());

			QVariant sr = cfgfile.override(parser, splitRecordingOption);
			server->setSplitRecording(sr.toBool());
		}
	}

	{
		QVariant hpv = cfgfile.override(parser, hostPassOption);
		if(!hpv.isNull())
			server->setHostPassword(hpv.toString());
	}

	{
		bool ok;
		int sessionLimit = cfgfile.override(parser, sessionLimitOption).toInt(&ok);
		if(!ok || sessionLimit<1) {
			logger::error() << "Invalid session count limit";
			return 1;
		}
		server->setSessionLimit(sessionLimit);
	}

	{
		bool persist = cfgfile.override(parser, persistentSessionOption).toBool();
		server->setPersistentSessions(persist);

		// Expiration time now works for non-persistent sessions as well.
		QString expire = cfgfile.override(parser, expireOption).toString();
		QRegularExpression re("\\A(\\d+(?:\\.\\d+)?)([dhms]?)\\z");
		auto m = re.match(expire);
		if(!m.hasMatch()) {
			logger::error() << "Invalid expiration time:" << expire;
			return 1;
		}

		float t = m.captured(1).toFloat();
		if(m.captured(2)=="d")
			t *= 24*60*60;
		else if(m.captured(2)=="h")
			t *= 60*60;
		else if(m.captured(2)=="m")
			t *= 60;

		server->setExpirationTime(t);
	}

	{
		QString sslCert = cfgfile.override(parser, sslCertOption).toString();
		QString sslKey = cfgfile.override(parser, sslKeyOption).toString();
		if(!sslCert.isEmpty() && !sslKey.isEmpty()) {
			server->setSslCertFile(sslCert, sslKey);
			server->setMustSecure(cfgfile.override(parser, secureOption).toBool());
			server::SslServer::requireForwardSecrecy();
		}
	}

#ifndef NDEBUG
	{
		uint lag = cfgfile.override(parser, lagOption).toUInt();
		server->setRandomLag(lag);
	}
#endif

	{
		QString userfile = cfgfile.override(parser, userfileOption).toString();
		if(!userfile.isEmpty())
			server->setUserFile(userfile);

		server->setAllowGuests(!cfgfile.override(parser, noGuestsOption).toBool());
	}

	{
		QString announceWhitelistFile = cfgfile.override(parser, announceWhitelist).toString();
		if(!announceWhitelistFile.isEmpty())
			server->setAnnounceWhitelist(announceWhitelistFile);
	}

	{
		QString announceLocalAddress = cfgfile.override(parser, announceLocalAddr).toString();
		if(!announceLocalAddress.isEmpty())
			server->setAnnounceLocalAddr(announceLocalAddress);
	}

	{
		QString banlistFile = cfgfile.override(parser, banlist).toString();
		if(!banlistFile.isEmpty())
			server->setBanlist(banlistFile);
	}
	{
		bool ok;
		float timeout = cfgfile.override(parser, timeoutOption).toFloat(&ok);
		if(!ok) {
			logger::error() << "invalid timeout";
			return 1;
		}
		server->setConnectionTimeout(timeout * 1000);
	}

	server->setPrivateUserList(cfgfile.override(parser, privateUserListOption).toBool());

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

			// TODO start webadmin if two fds were passsed
		}
	}


	initsys::notifyReady();

	return app.exec();
}

