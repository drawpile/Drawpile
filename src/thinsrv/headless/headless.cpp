// SPDX-License-Identifier: GPL-3.0-or-later
#include "cmake-config/config.h"
#include "libserver/inmemoryconfig.h"
#include "libserver/sslserver.h"
#include "libshared/util/paths.h"
#include "thinsrv/database.h"
#include "thinsrv/headless/configfile.h"
#include "thinsrv/initsys.h"
#include "thinsrv/multiserver.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QSslSocket>
#include <QStringList>
#include <memory>
#ifdef Q_OS_UNIX
#	include "thinsrv/headless/unixsignals.h"
#endif
#ifdef HAVE_WEBADMIN
#	include "thinsrv/webadmin/webadmin.h"
#endif
#ifdef HAVE_LIBSODIUM
#	include <sodium.h>
#endif

namespace server {
namespace headless {

namespace {
void printVersion()
{
	printf("drawpile-srv %s\n", cmake_config::version());
	printf(
		"Protocol version: %d.%d\n", cmake_config::proto::major(),
		cmake_config::proto::minor());
	printf(
		"Qt version: %s (compiled against %s)\n", qVersion(), QT_VERSION_STR);
	printf(
		"SSL library version: %s (%lu)\n",
		QSslSocket::sslLibraryVersionString().toLocal8Bit().constData(),
		QSslSocket::sslLibraryVersionNumber());
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
}

#ifdef HAVE_LIBSODIUM
bool isBadDrawpileNetExtAuthUrl(const QUrl &url)
{
	// Don't use "www.drawpile.net" or something.
	if(url.host().endsWith(
		   QStringLiteral(".drawpile.net"), Qt::CaseInsensitive)) {
		return true;
	} else if(
		url.host().compare(
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

bool isUncompiledOptionGiven(
	const QCommandLineParser &parser,
	const QVector<QPair<QString, QVector<const QCommandLineOption *>>> &options)
{
	bool error = false;
	for(const QPair<QString, QVector<const QCommandLineOption *>> &pair :
		options) {
		QStringList unavailableOptions;
		for(const QCommandLineOption *option : pair.second) {
			if(parser.isSet(*option)) {
				error = true;
				unavailableOptions.append(option->names().join('/'));
			}
		}
		if(!unavailableOptions.isEmpty()) {
			qCritical(
				"%s not compiled in, the following options are unavailable: %s",
				qUtf8Printable(pair.first),
				qUtf8Printable(unavailableOptions.join(QStringLiteral(", "))));
		}
	}
	return error;
}
}

bool start()
{
	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Standalone server for Drawpile");
	parser.addHelpOption();

	// --version, -v
	QCommandLineOption versionOption(
		QStringList() << "v" << "version", "Displays version information.");
	parser.addOption(versionOption);

#ifdef HAVE_SERVERGUI
	// --gui (this is just for the help text)
	QCommandLineOption guiOption(
		QStringList() << "gui", "Run the graphical version.");
	parser.addOption(guiOption);

	// --no-gui (dito)
	QCommandLineOption noGuiOption(
		QStringList() << "no-gui", "Run the headless version.");
	parser.addOption(noGuiOption);
#endif

	// --port, -p <port>
	QCommandLineOption portOption(
		QStringList() << "port" << "p", "Listening port", "port",
		QString::number(cmake_config::proto::port()));
	parser.addOption(portOption);

	// --listen, -l <address>
	QCommandLineOption listenOption(
		QStringList() << "listen" << "l", "Listening address", "address");
	parser.addOption(listenOption);

	QString websocketOptionSuffix;
	// --websocket-port <port>
	QCommandLineOption webSocketPortOption(
		QStringList() << "websocket-port",
		"WebSocket listening port (0 to disable)" + websocketOptionSuffix,
		"port", "0");
	parser.addOption(webSocketPortOption);

	// --websocket-listen <address>
	QCommandLineOption webSocketListenOption(
		QStringList() << "websocket-listen",
		"WebSocket listening address" + websocketOptionSuffix, "address");
	parser.addOption(webSocketListenOption);

	// --local-host
	QCommandLineOption localAddr(
		"local-host", "This server's hostname for session announcement",
		"hostname");
	parser.addOption(localAddr);

	// --announce-port <port>
	QCommandLineOption announcePortOption(
		QStringList() << "announce-port",
		"Port number to announce (set if forwarding from different port)",
		"port");
	parser.addOption(announcePortOption);

	// --ssl-cert <certificate file>
	QCommandLineOption sslCertOption(
		"ssl-cert", "SSL certificate file", "certificate");
	parser.addOption(sslCertOption);

	// --ssl-key <key file>
	QCommandLineOption sslKeyOption("ssl-key", "SSL key file", "key");
	parser.addOption(sslKeyOption);

	// --ssl-key-algorithm <algorithm>
	QCommandLineOption sslKeyAlgorithmOption(
		"ssl-key-algorithm",
		"SSL key algorithm: guess (the default), rsa, ec, dsa or dh.",
		"algorithm", "guess");
	parser.addOption(sslKeyAlgorithmOption);

	// --record <path>
	QCommandLineOption recordOption("record", "Record sessions", "path");
	parser.addOption(recordOption);

#ifdef HAVE_WEBADMIN
	QString webadminOptionSuffix;
#else
	QString webadminOptionSuffix = QStringLiteral(
		" [Libmicrohttpd not compiled in, this option is unavailable]");
#endif
	// --web-admin-port <port>
	QCommandLineOption webadminPortOption(
		"web-admin-port", "Web admin interface port" + webadminOptionSuffix,
		"port", "0");
	parser.addOption(webadminPortOption);

	// --web-admin-auth <user:password>
	QCommandLineOption webadminAuthOption(
		"web-admin-auth",
		"Web admin username & password (you can also use the "
		"DRAWPILESRV_WEB_ADMIN_AUTH environment variable for this)" +
			webadminOptionSuffix,
		"user:password");
	parser.addOption(webadminAuthOption);

	// --web-admin-access <address/subnet>
	QCommandLineOption webadminAccessOption(
		"web-admin-access", "Set web admin access mask" + webadminOptionSuffix,
		"address/subnet|all");
	parser.addOption(webadminAccessOption);

	// --web-admin-allow-origin <origin>
	QCommandLineOption webadminAllowedOriginOption(
		"web-admin-allowed-origin",
		"Allowed origin for Access-Control-Allow-Origin headers in responses" +
			webadminOptionSuffix,
		"origin");
	parser.addOption(webadminAllowedOriginOption);

	// --database, -d <filename>
	QCommandLineOption dbFileOption(
		QStringList() << "database" << "d", "Use configuration database",
		"filename");
	parser.addOption(dbFileOption);

	// --config, -c <filename>
	QCommandLineOption configFileOption(
		QStringList() << "config" << "c", "Use configuration file", "filename");
	parser.addOption(configFileOption);

	// --sessions, -s <path>
	QCommandLineOption sessionsOption(
		QStringList() << "sessions" << "s", "File backed sessions", "path");
	parser.addOption(sessionsOption);

	// --templates, -t <path>
	QCommandLineOption templatesOption(
		QStringList() << "templates" << "t", "Session templates", "path");
	parser.addOption(templatesOption);

#ifdef HAVE_LIBSODIUM
	QString sodiumOptionSuffix;
#else
	QString sodiumOptionSuffix = QStringLiteral(
		" [Libsodium not compiled in, this option is unavailable]");
#endif
	// --extauth <url>
	QCommandLineOption extAuthOption(
		QStringList() << "extauth", "Extauth server URL" + sodiumOptionSuffix,
		"url");
	parser.addOption(extAuthOption);

	// --crypt-key
	QCommandLineOption cryptKeyOption(
		QStringList() << "crypt-key",
		"Encryption key for session ban exports" + sodiumOptionSuffix, "key");
	parser.addOption(cryptKeyOption);

	// --generate-crypt-key
	QCommandLineOption generateCryptKeyOption(
		QStringList() << "generate-crypt-key",
		"Generate a key to pass to --crypt-key and exit" + sodiumOptionSuffix);
	parser.addOption(generateCryptKeyOption);

	// --report-url <url>
	QCommandLineOption reportUrlOption(
		QStringList() << "report-url", "Abuse report handler URL", "url");
	parser.addOption(reportUrlOption);

	// Parse
	parser.process(*QCoreApplication::instance());

	if(parser.isSet(versionOption)) {
		printVersion();
		::exit(0);
	}

	// Give nice error messages about parameters for stuff not compiled in.
	if(isUncompiledOptionGiven(
		   parser,
		   {
#ifndef HAVE_WEBADMIN
			   {QStringLiteral("Libmicrohttpd"),
				{&webadminPortOption, &webadminAuthOption,
				 &webadminAccessOption, &webadminAllowedOriginOption}},
#endif
#ifndef HAVE_LIBSODIUM
			   {QStringLiteral("Libsodium"),
				{&extAuthOption, &cryptKeyOption, &generateCryptKeyOption}},
#endif
		   })) {
		return false;
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

	if(int listenFdCount = initsys::getListenFds().size()) {
		qCritical(
			"Got %d file descriptor(s), but socket activation is no longer "
			"supported. Uninstall %s from %s and try again.",
			listenFdCount, qUtf8Printable(initsys::name()),
			qUtf8Printable(initsys::socketActivationName()));
		::exit(1);
	}

	// Set server configuration file or database
	std::unique_ptr<ServerConfig> serverconfig;
	if(parser.isSet(dbFileOption)) {
		if(parser.isSet(configFileOption)) {
			qCritical("Configuration file and database are mutually exclusive "
					  "options");
			return false;
		}

		Database *db = new Database;
		serverconfig.reset(db);
		if(!db->openFile(parser.value(dbFileOption))) {
			qCritical(
				"Couldn't open database file %s",
				qPrintable(parser.value(dbFileOption)));
			return false;
		}

	} else if(parser.isSet(configFileOption)) {
		serverconfig.reset(new ConfigFile(parser.value(configFileOption)));

	} else {
		// No database or config file: just use the defaults
		serverconfig.reset(new InMemoryConfig);
	}

	// Set internal server config
	InternalConfig icfg;
	icfg.setLocalHostname(parser.value(localAddr));
#ifdef HAVE_LIBSODIUM
	QString extAuthUrl = parser.value(extAuthOption);
	if(!extAuthUrl.isEmpty()) {
		icfg.extAuthUrl = QUrl(extAuthUrl, QUrl::StrictMode);
		if(!validateExtAuthUrl(extAuthUrl, icfg.extAuthUrl)) {
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
			return false;
		}
	}
#endif
	icfg.reportUrl = parser.value(reportUrlOption);

	if(parser.isSet(announcePortOption)) {
		bool ok;
		icfg.announcePort = parser.value(announcePortOption).toInt(&ok);
		if(!ok || icfg.announcePort > 0xffff) {
			qCritical(
				"Invalid port %s",
				qPrintable(parser.value(announcePortOption)));
			return false;
		}
	}

	serverconfig->setInternalConfig(icfg);

	// Initialize the server
	std::unique_ptr<server::MultiServer> server(
		new server::MultiServer(serverconfig.release()));

	int port;
	{
		bool ok;
		port = parser.value(portOption).toInt(&ok);
		if(!ok || port < 1 || port > 0xffff) {
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

	int webSocketPort;
	{
		bool ok;
		webSocketPort = parser.value(webSocketPortOption).toInt(&ok);
		if(!ok || webSocketPort < 0 || webSocketPort > 0xffff) {
			qCritical(
				"Invalid WebSocket port %s",
				qUtf8Printable(parser.value(webSocketPortOption)));
			return false;
		}
	}
	QHostAddress webSocketAddress = QHostAddress::Any;
	{
		QString av = parser.value(webSocketListenOption);
		if(!av.isEmpty()) {
			if(!webSocketAddress.setAddress(av)) {
				qCritical(
					"Invalid WebSocket listening address %s",
					qUtf8Printable(av));
				return false;
			}
		}
	}

	{
		QString sslCert = parser.value(sslCertOption);
		QString sslKey = parser.value(sslKeyOption);
		QString sslKeyAlgorithm = parser.value(sslKeyAlgorithmOption);
		if(!sslCert.isEmpty() && !sslKey.isEmpty()) {
			server::SslServer::Algorithm algorithm;
			if(sslKeyAlgorithm.compare("guess", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Guess;
			} else if(
				sslKeyAlgorithm.compare("rsa", Qt::CaseInsensitive) == 0) {
				algorithm = SslServer::Algorithm::Rsa;
			} else if(
				sslKeyAlgorithm.compare("dsa", Qt::CaseInsensitive) == 0) {
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
			QDir sessionDir{sessionDirPath};
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
			qCritical(
				"%s: template directory does not exist!",
				qPrintable(dir.absolutePath()));
			return false;
		}
		server->setTemplateDirectory(dir);
	}

#ifdef HAVE_WEBADMIN
	server::Webadmin *webadmin = new server::Webadmin(server.get());
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

#	ifdef Q_OS_UNIX
		server->connect(
			UnixSignals::instance(), &UnixSignals::sigUsr1, webadmin,
			&Webadmin::restart);
#	endif
	}

#endif

#ifdef Q_OS_UNIX
	// Catch signals
	server->connect(
		UnixSignals::instance(), &UnixSignals::sigInt, server.get(),
		&MultiServer::stop);
	server->connect(
		UnixSignals::instance(), &UnixSignals::sigTerm, server.get(),
		&MultiServer::stop);
#endif

	if(!server->start(port, address, webSocketPort, webSocketAddress)) {
		return false;
	}

#ifdef HAVE_WEBADMIN
	if(webadminPort > 0) {
		webadmin->setSessions(server.get());
		QString webadminDirectory = utils::paths::locateDataFile("webadmin/");
		if(!webadminDirectory.isEmpty()) {
			webadmin->setStaticFileRoot(QDir(webadminDirectory));
		}
		webadmin->start(webadminPort);
	}
#endif

	server->connect(
		server.get(), &MultiServer::serverStopped, server.get(),
		&MultiServer::deleteLater);
	server->connect(
		server.get(), &MultiServer::destroyed, QCoreApplication::instance(),
		&QCoreApplication::quit, Qt::QueuedConnection);

	server.release(); // We're up and running, so don't delete the server.
	return true;
}

}
}
