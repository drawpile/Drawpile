/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <iostream>
#include <QApplication>
#include <QSettings>
#include <QUrl>

#include "mainwindow.h"
#include "localserver.h"

int main(int argc, char *argv[]) {
	QApplication app(argc,argv);

	// These are used by QSettings
	app.setOrganizationName("DrawPile");
	app.setOrganizationDomain("drawpile.sourceforge.net");
	app.setApplicationName("DrawPile");

	// Create the local server handler
	LocalServer *srv = LocalServer::getInstance();
	app.connect(&app, SIGNAL(aboutToQuit()), srv, SLOT(shutdown()));

	// Create and show the main window
	MainWindow win;

	QStringList args = app.arguments();
	if(args.count()>1) {
		QString arg = args.at(1);
		// Parameter given, we assume it to be either an URL to a session
		// or a filename.
		if(arg.startsWith("drawpile://")) {
			// Create a default board first, in case connection fails
			win.initBoard(QSize(800,600), Qt::white);
			// Join the session
			QUrl url(arg, QUrl::TolerantMode);
			if(url.userName().isEmpty()) {
				// Set username if not specified
				QSettings cfg;
				cfg.beginGroup("network");
				QString defaultname = getenv("USER");
				if(defaultname.isEmpty())
					defaultname = "user";
				url.setUserName(cfg.value("username", defaultname).toString());
			}
			win.joinSession(url);
		} else {
			if(win.initBoard(argv[1])==false) {
				std::cerr << argv[1] << ": couldn't load image.\n";
				return 1;
			}
		}
	} else {
		// Create a default board
		win.initBoard(QSize(800,600), Qt::white);
	}
	win.show();

	return app.exec();
}

