// SPDX-License-Identifier: GPL-3.0-or-later

#include "config.h"

#include "libserver/jsonapi.h" // for datatype registration

#include "thinsrv/initsys.h"
#include "thinsrv/headless/headless.h"

#ifdef HAVE_SERVERGUI
#include "thinsrv/gui/gui.h"
#include <QApplication>

#else
#include <QCoreApplication>
#endif

#include <cstdio>
#include <cstring>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif


int main(int argc, char *argv[]) {
#ifdef Q_OS_UNIX
	// Security check
	if(geteuid() == 0) {
		fprintf(stderr, "This program should not be run as root!\n");
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
	bool useGui = false;
#ifdef HAVE_SERVERGUI
	if(initsys::getListenFds().isEmpty()) {
		useGui = argc==1;
		for(int i=1;i<argc;++i) {
			if(strcmp(argv[i], "--gui")==0) {
				useGui = true;
				break;
			}
		}
	}

	if(useGui)
		app.reset(new QApplication(argc, argv));
	else
#endif
		app.reset(new QCoreApplication(argc, argv));


	// Set common settings
	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	// Start the server
	if(useGui) {
#ifdef HAVE_SERVERGUI
		if(!server::gui::start())
			return 1;
#endif
	} else {
		if(!server::headless::start())
			return 1;
	}

	initsys::notifyReady();
	return app->exec();
}

