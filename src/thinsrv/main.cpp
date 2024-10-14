// SPDX-License-Identifier: GPL-3.0-or-later
#include "cmake-config/config.h"
#include "libserver/jsonapi.h" // for datatype registration
#include "thinsrv/headless/headless.h"
#include "thinsrv/initsys.h"
#include <cstdio>
#include <cstring>
#ifdef HAVE_SERVERGUI
#	include "thinsrv/gui/gui.h"
#	include <QApplication>
#else
#	include <QCoreApplication>
#endif
#ifdef Q_OS_UNIX
#	include <unistd.h>
#endif


int main(int argc, char *argv[])
{
#ifdef Q_OS_UNIX
	// Security check
	if(geteuid() == 0) {
		std::fprintf(stderr, "This program should not be run as root!\n");
		return 1;
	}
#endif

	QScopedPointer<QCoreApplication> app;

	qRegisterMetaType<server::JsonApiMethod>("JsonApiMethod");
	qRegisterMetaType<server::JsonApiResult>("JsonApiResult");

// GUI version is started if:
//  * --gui command line argument is given
//  * or no command line arguments are given
//  * and listening fds were not passed by an init system
#ifdef HAVE_SERVERGUI
	bool useGui = false;
	if(initsys::getListenFds().isEmpty()) {
		useGui = argc == 1;
		for(int i = 1; i < argc; ++i) {
			if(strcmp(argv[i], "--gui") == 0) {
				useGui = true;
				break;
			}
		}
	}

	if(useGui) {
		app.reset(new QApplication(argc, argv));
	} else {
		app.reset(new QCoreApplication(argc, argv));
	}
#else
	app.reset(new QCoreApplication(argc, argv));
#endif


	// Set common settings
	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(cmake_config::version());

	// Start the server
#ifdef HAVE_SERVERGUI
	bool started = useGui ? server::gui::start() : server::headless::start();
#else
	bool started = server::headless::start();
#endif
	if(!started) {
		return 1;
	}

	initsys::notifyReady();

	return app->exec();
}
