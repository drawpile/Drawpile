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
#include <QMessageBox>

#include "mainwindow.h"
#include "localserver.h"

// Set default username
static void initUsername() {
	QSettings cfg;
	cfg.beginGroup("history");
	if(cfg.contains("username") == false ||
			cfg.value("username").toString().isEmpty())
	{
#ifdef Q_WS_WIN
		QString defaultname = getenv("USERNAME");
#else
		QString defaultname = getenv("USER");
#endif

		cfg.setValue("username", defaultname);
	}
}

int main(int argc, char *argv[]) {
	QApplication app(argc,argv);

	// These are used by QSettings
	app.setOrganizationName("DrawPile");
	app.setOrganizationDomain("drawpile.sourceforge.net");
	app.setApplicationName("DrawPile");

#ifdef Q_WS_WIN
	{
		// Initialze QSettings to use the .ini file format on Windows.
		QSettings cfg(QSettings::IniFormat, QSettings::UserScope,
				app.organizationName(), app.applicationName());
	}

#endif
	initUsername();

	// Create the local server handler
	LocalServer *srv = LocalServer::getInstance();
	app.connect(&app, SIGNAL(aboutToQuit()), srv, SLOT(shutdown()));

	// Create the main window
	MainWindow *win = new MainWindow;

	const QStringList args = app.arguments();
	if(args.count()>1) {
		const QString arg = args.at(1);
		// Parameter given, we assume it to be either an URL to a session
		// or a filename.
		if(arg.startsWith("drawpile://")) {
			// Create a default board first, in case connection fails
			win->initDefaultBoard();
			// Join the session
			QUrl url(arg, QUrl::TolerantMode);
			if(url.userName().isEmpty()) {
				// Set username if not specified
				QSettings cfg;
				cfg.beginGroup("history");
				url.setUserName(cfg.value("username").toString());
			}
			win->joinSession(url);
		} else {
			if(win->initBoard(argv[1])==false) {
				// If image couldn't be loaded, initialize to a default board
				// and show error message.
				win->initDefaultBoard();
				QMessageBox::warning(win, app.tr("DrawPile"),
						app.tr("Couldn't load image %1.").arg(argv[1]));
			}
		}
	} else {
		// Create a default board
		win->initDefaultBoard();
	}
	win->show();

	return app.exec();
}

