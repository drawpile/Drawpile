// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/multiserver.h"
#include "thinsrv/initsys.h"
#include "thinsrv/database.h"
#include "libserver/sslserver.h"
#include "libserver/inmemoryconfig.h"
#include "thinsrv/headless/configfile.h"
#include "libshared/util/paths.h"
#include "cmake-config/config.h"

#ifdef HAVE_WEBADMIN
#include "thinsrv/webadmin/webadmin.h"
#endif

#include <QCoreApplication>
#include <QStringList>
#include <QSslSocket>
#include <QCommandLineParser>
#include <QDir>

#ifdef Q_OS_UNIX
#include "thinsrv/headless/unixsignals.h"
#endif

#ifdef HAVE_LIBSODIUM
#include <sodium.h>
#endif

namespace server {
namespace headless {

namespace {
void printVersion()
{
	printf("drawpile-srv %s\n", cmake_config::version());
	printf("Protocol version: %d.%d\n", cmake_config::proto::major(), cmake_config::proto::minor());
	printf("Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
	printf("SSL library version: %s (%lu)\n", QSslSocket::sslLibraryVersionString().toLocal8Bit().constData(), QSslSocket::sslLibraryVersionNumber());
#ifdef HAVE_WEBADMIN
	printf("Libmicrohttpd version: %s\n", qPrintable(Webadmin::version()));
#else
	printf("Libmicrohttpd version: N/A (not compiled in)\n");
#endif
#ifdef HAVE_LIBSODIUM
	printf("Libsodium version: %s\n", sodium_version_string());
#else
	printf("Libsodium version: N/A (not compiled in)\n");
#endif
#ifdef HAVE_WEBSOCKETS
	printf("QtWebSockets: yes\n");
#else
	printf("QtWebSockets: N/A (not compiled in)\n");
#endif
}

#ifdef HAVE_LIBSODIUM
bool isBadDrawpileNetExtAuthUrl(const QUrl &url)
{
	// Don't use "www.drawpile.net" or something.
	if(url.host().endsWith(
		   QStringLiteral(".drawpile.net"), Qt::CaseInsensitive)) {
		return true;
	} else if(url.host().compare(
				  QStringLiteral("drawpile.net"), Qt::CaseInsensitive) == 0) {
		// The URL should be "https://drawpile.net/api/ext-auth/". Don't allow
		// funny capitalization, a scheme other than https, a missing slash or
		// anything else.  Specifying port 443 explicitly, adding query
		// parameters or a fragment are okay though.
		return url.host() != QStringLiteral("drawpile.net") ||
			   (url.port() >= 0 && url.port() != 443) ||
			   url.scheme() != QStringLiteral("https") ||
			   url.path() != QStringLiteral("/api/ext-auth/");
	} else {
		return false;
	}
}

bool validateExtAuthUrl(const QString &param, const QUrl &url)
{
	if(!url.isValid()) {
		qCritical(
			"Invalid extauthurl '%s': %s", qUtf8Printable(param),
			qUtf8Printable(url.errorString()));
		return false;
	}

	QString scheme = url.scheme();
	if(scheme != QStringLiteral("http") && scheme != QStringLiteral("https")) {
		qCritical(
			"Invalid extauthurl '%s': must start with https: or http:",
			qUtf8Printable(param));
		return false;
	}

	if(isBadDrawpileNetExtAuthUrl(url)) {
		qCritical(
			"Bad drawpile.net extauthurl '%s', should be "
			"'https://drawpile.net/api/ext-auth/'",
			qUtf8Printable(param));
		return false;
	}

	return true;
}
#endif
}

bool start() {
	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Standalone server for Drawpile");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

#ifdef HAVE_SERVERGUI
	// --gui (this is just for the help text)
	QCommandLineOption guiOption(QStringList() << "gui", "Run the graphical version.");
	parser.addOption(guiOption);
#endif

	// --port, -p <port>
	QCommandLineOption portOption(QStringList() << "port" << "p", "Listening port", "port", QString::number(cmake_config::proto::port()));
	parser.addOption(portOption);

	// --listen, -l <address>
	QCommandLineOption listenOption(QStringList() << "listen" << "l", "Listening address", "address");
	parser.addOption(listenOption);

#ifdef HAVE_WEBSOCKETS
	// --websocket-port <port>
	QCommandLineOption webSocketPortOption(QStringList() << "websocket-port", "WebSocket listening port (0 to disable)", "port", "0");
	parser.addOption(webSocketPortOption);

	// --websocket-listen <address>
	QCommandLineOption webSocketListenOption(QStringList() << "websocket-listen", "WebSocket listening address", "address");
	parser.addOption(webSocketListenOption);
#endif

	// --local-host
	QCommandLineOption localAddr("local-host", "This server's hostname for session announcement", "hostname");
	parser.addOption(localAddr);

	// --announce-port <port>
	QCommandLineOption announcePortOption(QStringList() << "announce-port", "Port number to announce (set if forwarding from different port)", "port");
	parser.addOption(announcePortOption);

	// --ssl-cert <certificate file>
	QCommandLineOption sslCertOption("ssl-cert", "SSL certificate file", "certificate");
	parser.addOption(sslCertOption);

	// --ssl-key <key file>
	QCommandLineOption sslKeyOption("ssl-key", "SSL key file", "key");
	parser.addOption(sslKeyOption);

	// --ssl-key-algorithm <algorithm>
	QCommandLineOption sslKeyAlgorithmOption("ssl-key-algorithm", "SSL key algorithm: guess (the default), rsa, ec, dsa or dh.", "algorithm", "guess");
	parser.addOption(sslKeyAlgorithmOption);

	// --record <path>
	QCommandLineOption recordOption("record", "Record sessions", "path");
	parser.addOption(recordOption);

#ifdef HAVE_WEBADMIN
	// --web-admin-port <port>
	QCommandLineOption webadminPortOption("web-admin-port", "Web admin interface port", "port", "0");
	parser.addOption(webadminPortOption);

	// --web-admin-auth <user:password>
	QCommandLineOption webadminAuthOption("web-admin-auth", "Web admin username & password (you can also use the DRAWPILESRV_WEB_ADMIN_AUTH environment variable for this)", "user:password");
	parser.addOption(webadminAuthOption);

	// --web-admin-access <address/subnet>
	QCommandLineOption webadminAccessOption("web-admin-access", "Set web admin access mask", "address/subnet|all");
	parser.addOption(webadminAccessOption);

	// --web-admin-allow-origin <origin>
	QCommandLineOption webadminAllowedOriginOption(
		"web-admin-allowed-origin",
		"Allowed origin for Access-Control-Allow-Origin headers in responses",
		"origin");
	parser.addOption(webadminAllowedOriginOption);
#endif

	// --database, -d <filename>
	QCommandLineOption dbFileOption(QStringList() << "database" << "d", "Use configuration database", "filename");
	parser.addOption(dbFileOption);

	// --config, -c <filename>
	QCommandLineOption configFileOption(QStringList() << "config" << "c", "Use configuration file", "filename");
	parser.addOption(configFileOption);

	// --sessions, -s <path>
	QCommandLineOption sessionsOption(QStringList() << "sessions" << "s", "File backed sessions", "path");
	parser.addOption(sessionsOption);

	// --templates, -t <path>
	QCommandLineOption templatesOption(QStringList() << "templates" << "t", "Session templates", "path");
	parser.addOption(templatesOption);

#ifdef HAVE_LIBSODIUM
	// --extauth <url>
	QCommandLineOption extAuthOption(QStringList() << "extauth", "Extauth server URL", "url");
	parser.addOption(extAuthOption);

	// --crypt-key
	QCommandLineOption cryptKeyOption(QStringList() << "crypt-key", "Encryption key for session ban exports", "key");
	parser.addOption(cryptKeyOption);

	// --generate-crypt-key
	QCommandLineOption generateCryptKeyOption(QStringList() << "generate-crypt-key", "Generate a key to pass to --crypt-key and exit");
	parser.addOption(generateCryptKeyOption);
#endif

	// --report-url <url>
	QCommandLineOption reportUrlOption(QStringList() << "report-url", "Abuse report handler URL", "url");
	parser.addOption(reportUrlOption);

	// Parse
	parser.process(*QCoreApplication::instance());

	if(parser.isSet(versionOption)) {
		printVersion();
		::exit(0);
	}

#ifdef HAVE_LIBSODIUM
	if(parser.isSet(generateCryptKeyOption)) {
		if(sodium_init() < 0) {
			qCritical("Failed to initialize sodium");
			::exit(1);
		}
		QByteArray key(crypto_secretbox_KEYBYTES, 0);
		randombytes_buf(key.data(), key.size());
		puts(key.toBase64().constData());
		::exit(0);
	}
#endif

	// Set server configuration file or database
	ServerConfig *serverconfig;
	if(parser.isSet(dbFileOption)) {
		if(parser.isSet(configFileOption)) {
			qCritical("Configuration file and database are mutually exclusive options");
			return false;
		}

		auto *db = new Database;
		if(!db->openFile(parser.value(dbFileOption))) {
			qCritical("Couldn't open database file %s", qPrintable(parser.value(dbFileOption)));
			delete db;
			return false;
		}
		serverconfig = db;

	} else if(parser.isSet(configFileOption)) {
		serverconfig = new ConfigFile(parser.value(configFileOption));

	} else {
		// No database or config file: just use the defaults
		serverconfig = new InMemoryConfig;
	}

	// Set internal server config
	InternalConfig icfg;
	icfg.localHostname = parser.value(localAddr);
#ifdef HAVE_LIBSODIUM
	QString extAuthUrl = parser.value(extAuthOption);
	if(!extAuthUrl.isEmpty()) {
		icfg.extAuthUrl = QUrl(extAuthUrl, QUrl::StrictMode);
		if(!validateExtAuthUrl(extAuthUrl, icfg.extAuthUrl)) {
			delete serverconfig;
			return false;
		}
	}

	QString cryptKey = parser.value(cryptKeyOption);
	if(!cryptKey.isEmpty()) {
		QByteArray decodedKey = QByteArray::fromBase64(cryptKey.toUtf8());
		if(decodedKey.size() == crypto_secretbox_KEYBYTES) {
			icfg.cryptKey = decodedKey;
			if(sodium_init() < 0) {
				qCritical("Failed to initialize sodium");
				return false;
			}
		} else {
			qCritical("Invalid crypt key '%s'", qUtf8Printable(cryptKey));
			qInfo("Use --generate-crypt-key to generate a proper one.");
			delete serverconfig;
			return false;
		}
	}
#endif
	icfg.reportUrl = parser.value(reportUrlOption);

	if(parser.isSet(announcePortOption)) {
		bool ok;
		icfg.announcePort = parser.value(announcePortOption).toInt(&ok);
		if(!ok || icfg.announcePort>0xffff) {
			qCritical("Invalid port %s", qPrintable(parser.value(announcePortOption)));
			delete serverconfig;
			return false;
		}
	}

	serverconfig->setInternalConfig(icfg);

	// Initialize the server
	server::MultiServer *server = new server::MultiServer(serverconfig);
	serverconfig->setParent(server);

	server->connect(server, SIGNAL(serverStopped()), QCoreApplication::instance(), SLOT(quit()));

	int port;
	{
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port<1 || port>0xffff) {
			qCritical("Invalid port %s", qPrintable(parser.value(portOption)));
			return false;
		}
	}

	QHostAddress address = QHostAddress::Any;
	{
		QString av = parser.value(listenOption);
		if(!av.isEmpty()) {
			if(!address.setAddress(av)) {
				qCritical("Invalid listening address %s", qPrintable(av));
				return false;
			}
		}
	}

#ifdef HAVE_WEBSOCKETS
	int webSocketPort;
	{
		bool ok;
		webSocketPort = parser.value(webSocketPortOption).toInt(&ok);
		if(!ok || webSocketPort<0 || webSocketPort>0xffff) {
			qCritical("Invalid WebSocket port %s", qUtf8Printable(parser.value(webSocketPortOption)));
			return false;
		}
	}
	QHostAddress webSocketAddress = QHostAddress::Any;
	{
		QString av = parser.value(webSocketListenOption);
		if(!av.isEmpty()) {
			if(!webSocketAddress.setAddress(av)) {
				qCritical("Invalid WebSocket listening address %s", qUtf8Printable(av));
				return false;
			}
		}
	}
#else
	const int webSocketPort = 0;
	const QHostAddress webSocketAddress = QHostAddress::Any;
#endif

	{
		QString sslCert = parser.value(sslCertOption);
		QString sslKey = parser.value(sslKeyOption);
		QString sslKeyAlgorithm = parser.value(sslKeyAlgorithmOption);
		if(!sslCert.isEmpty() && !sslKey.isEmpty()) {
			server::SslServer::Algorithm algorithm;
			if(sslKeyAlgorithm.compare("guess", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Guess;
			} else if(sslKeyAlgorithm.compare("rsa", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Rsa;
			} else if(sslKeyAlgorithm.compare("dsa", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Dsa;
			} else if(sslKeyAlgorithm.compare("ec", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Ec;
			} else if(sslKeyAlgorithm.compare("dh", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Dh;
			} else {
				qCritical(
					"Invalid ssl-key-algorithm '%s', must be one of 'guess', "
					"'rsa', 'dsa', 'ec', 'dh'",
					qUtf8Printable(sslKeyAlgorithm));
				return false;
			}
			server->setSslCertFile(sslCert, sslKey, algorithm);
		}
	}

	{
		QString recordingPath = parser.value(recordOption);
		if(!recordingPath.isEmpty()) {
			server->setRecordingPath(recordingPath);
		}
	}

	{
		QString sessionDirPath = parser.value(sessionsOption);
		if(!sessionDirPath.isEmpty()) {
			QDir sessionDir { sessionDirPath };
			sessionDir.mkpath(".");
			if(!sessionDir.isReadable()) {
				qCritical("Cannot open %s", qPrintable(sessionDirPath));
				return false;
			} else {
				server->setSessionDirectory(sessionDir);
			}
		}
	}

	if(parser.isSet(templatesOption)) {
		QDir dir(parser.value(templatesOption));
		if(!dir.exists()) {
			qCritical("%s: template directory does not exist!", qPrintable(dir.absolutePath()));
			return false;
		}
		server->setTemplateDirectory(dir);
	}

#ifdef HAVE_WEBADMIN
	server::Webadmin *webadmin = new server::Webadmin;
	int webadminPort = parser.value(webadminPortOption).toInt();
	{
		QString auth = parser.value(webadminAuthOption);
		if(auth.isEmpty()) {
			auth = qEnvironmentVariable("DRAWPILESRV_WEB_ADMIN_AUTH");
		}
		if(!auth.isEmpty()) {
			webadmin->setBasicAuth(auth);
		}

		QString access = parser.value(webadminAccessOption);
		if(!access.isEmpty()) {
			if(!webadmin->setAccessSubnet(access)) {
				qCritical("Invalid subnet %s", qPrintable(access));
				return false;
			}
		}

		QString allowedOrigin = parser.value(webadminAllowedOriginOption);
		if(!allowedOrigin.isEmpty()) {
			webadmin->setAllowedOrigin(allowedOrigin);
		}

#ifdef Q_OS_UNIX
	server->connect(UnixSignals::instance(), SIGNAL(sigUsr1()), webadmin, SLOT(restart()));
#endif
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
			if(!server->start(port, address, webSocketPort, webSocketAddress)) {
				return false;
			}

#ifdef HAVE_WEBADMIN
			if(webadminPort>0) {
				webadmin->setSessions(server);
				const auto webadminDirectory = utils::paths::locateDataFile("webadmin/");
				if(!webadminDirectory.isEmpty()) {
					webadmin->setStaticFileRoot(QDir(webadminDirectory));
				}
				webadmin->start(webadminPort);
			}
#endif

		} else {
			const QCommandLineOption *potentiallyIgnoredOptions[] = {
				&portOption,
#ifdef HAVE_WEBADMIN
				&webadminPortOption,
#endif
#ifdef HAVE_WEBSOCKETS
				&webSocketPortOption,
				&webSocketListenOption,
#endif
			};
			QStringList ignoredOptions;
			for(const QCommandLineOption *option : potentiallyIgnoredOptions) {
				if(parser.isSet(*option)) {
					QStringList optionNames;
					for(const QString &optionName : option->names()) {
						QString prefix = optionName.length() == 1
											 ? QStringLiteral("-")
											 : QStringLiteral("--");
						optionNames.append(prefix + optionName);
					}
					ignoredOptions.append(optionNames.join('/'));
				}
			}

			// listening socket passed to us by the init system
			int fdCount = listenfds.size();
			if(fdCount > 3) {
				qCritical("Too many file descriptors received");
				return false;
			}

			int fdTcp = listenfds[0];
			int fdWebAdmin = fdCount < 2 ? -1 : listenfds[1];
			int fdWebSocket = fdCount < 3 ? -1 : listenfds[2];

			server->setAutoStop(true);

#ifndef HAVE_WEBSOCKETS
			if(fdWebSocket > 0) {
				qCritical("WebSocket passed, but support not built in!");
			}
#endif
			if(!server->startFd(fdTcp, fdWebSocket, ignoredOptions)) {
				return false;
			}

			if(fdWebAdmin > 0) {
#ifdef HAVE_WEBADMIN
				webadmin->setSessions(server);
				webadmin->startFd(listenfds[1]);
#else
				qCritical("Web admin socket passed, but web admin support not built in!");
#endif
			}
		}
	}

	return true;
}

}
}
