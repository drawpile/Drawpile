/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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

	QCoreApplication *app;

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
		app = new QApplication(argc, argv);
	else
#endif
		app = new QCoreApplication(argc, argv);


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

