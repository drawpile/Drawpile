// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/gui/mainwindow.h"
#include "thinsrv/gui/localserver.h"
#include "thinsrv/gui/remoteserver.h"
#include "thinsrv/multiserver.h"
#include "thinsrv/database.h"
#include "thinsrv/gui/trayicon.h"
#include "thinsrv/gui/singleinstance.h"
#include "thinsrv/gui/authdialog.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QThread>
#include <QMessageBox>
#include <QMutex>
#include <QFileInfo>
#include <QStandardPaths>
#include <QDir>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QMenu>

namespace server {
namespace gui {

bool startServer()
{
	QString errorMessage;
	MultiServer *server=nullptr;

	// A database is always used with the GUI server
	QDir dbDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
	dbDir.mkpath(".");
	QString dbFile = QFileInfo(dbDir, "guiserver.db").absoluteFilePath();

	// The server must be initialized in its own thread
	QMutex mutex;
	auto threadInitFunc = [dbFile, &errorMessage, &server, &mutex]() {
		Database *db = new Database;
		if(!db->openFile(dbFile)) {
			errorMessage = QApplication::tr("Couldn't open database");
			mutex.unlock();
			return;
		}

		server = new MultiServer(db);
		db->setParent(server);

		mutex.unlock();
	};

	// Start a new thread for the server
	qDebug() << "Starting server thread";
	QThread *serverThread = new QThread;
	auto initConnetion = serverThread->connect(serverThread, &QThread::started, threadInitFunc);
	mutex.lock();
	serverThread->start();

	// Wait for initialization to complete
	mutex.lock();
	mutex.unlock();
	QObject::disconnect(initConnetion);
	qDebug() << "Server thread initialized";

	if(!errorMessage.isEmpty()) {
		QMessageBox::critical(nullptr, QApplication::tr("Drawpile Server"), errorMessage);
		return false;
	}

	Q_ASSERT(server);

	// The local server connector to use
	LocalServer *localServer = new LocalServer(server);

	QObject::connect(localServer, &LocalServer::serverError, [](const QString &error) {
		QMessageBox::warning(nullptr, QApplication::tr("Drawpile Server"), error);
	});

	// Create the system tray menu
	if(QSettings().value("ui/trayicon", true).toBool()) {
		TrayIcon::showTrayIcon();
	}

	QObject::connect(server, &MultiServer::userCountChanged, TrayIcon::instance(), &TrayIcon::setNumber);
	QObject::connect(localServer, &LocalServer::serverStateChanged, TrayIcon::instance(), &TrayIcon::setServerOn);

	// Open the main window
	MainWindow::setDefaultInstanceServer(localServer);
	MainWindow::showDefaultInstance();

	// Quit when last window is closed, but not if the tray icon is visible
	qApp->setQuitOnLastWindowClosed(false);
#ifndef Q_OS_MACOS
	QObject::connect(qApp, &QApplication::lastWindowClosed, []() {
		if(!TrayIcon::isTrayIconVisible())
			qApp->quit();
	});
#endif

	return true;
}

bool startRemote(const QString &address)
{
	QUrl url(address);
	if(!url.isValid()) {
		QMessageBox::critical(nullptr, QApplication::tr("Drawpile Server"), QApplication::tr("Invalid URL"));
		return false;
	}

	AuthDialog::init();

	RemoteServer *remote = new RemoteServer(url);

	MainWindow *win = new MainWindow(remote);
	remote->setParent(win);

	win->show();
	return true;
}

bool start() {
	// Set up command line arguments
	QCommandLineParser parser;

	parser.setApplicationDescription("Standalone server for Drawpile (graphical mode)");
	parser.addHelpOption();

	// --gui (this is just for the help text)
	QCommandLineOption guiOption(QStringList() << "gui", "Run the graphical version.");
	parser.addOption(guiOption);

	// remote <address>
	QCommandLineOption remoteOption(QStringList() << "remote", "Remote admin mode", "address");
	parser.addOption(remoteOption);

	// --dump-config
	QCommandLineOption dumpConfigOption(QStringList() << "dump-config", "Dump config and exit");
	parser.addOption(dumpConfigOption);

	// Parse
	parser.process(*QCoreApplication::instance());

	if(parser.isSet(remoteOption)) {
		// Remote admin mode
		return startRemote(parser.value(remoteOption));

	} else {
		// Normal server mode
		SingleInstance *guard = new SingleInstance;
		if(!guard->tryStart()) {
			qWarning("Another instance is already running");
			return false;
		}
		QObject::connect(qApp, &QApplication::aboutToQuit, guard, &SingleInstance::deleteLater);

		return startServer();
	}
}

}
}
