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

#include "initsys.h"
#include "headless/headless.h"

#include <QCoreApplication>
#include <cstdio>

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

	// TODO: decide here whether to run the headless or GUI version
	// GUI version is started if:
	//  * --gui command line argument is given
	//  * or no command line arguments are given
	//  * and listening fds were not passed by an init system
	app = new QCoreApplication(argc, argv);

	// Set common settings
	QCoreApplication::setOrganizationName("drawpile");
	QCoreApplication::setOrganizationDomain("drawpile.net");
	QCoreApplication::setApplicationName("drawpile-srv");
	QCoreApplication::setApplicationVersion(DRAWPILE_VERSION);

	initsys::setInitSysLogger();

	// Start the server
	if(!server::headless::start())
		return 1;

	initsys::notifyReady();
	return app->exec();
}

