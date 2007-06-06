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

#include "main.h"
#include "mainwindow.h"
#include "localserver.h"

DrawPileApp::DrawPileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("DrawPile");
	setOrganizationDomain("drawpile.sourceforge.net");
	setApplicationName("DrawPile");

	// Make sure a user name is set
	QSettings& cfg = getSettings();
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

	// Create the local server handler
	server_ = new LocalServer();
	connect(this, SIGNAL(aboutToQuit()), server_, SLOT(shutdown()));

}

QSettings& DrawPileApp::getSettings()
{
#ifdef Q_WS_WIN
	// Use .ini files on windows
	static QSettings cfg(QSettings::IniFormat, QSettings::UserScope,
			organizationName(), applicationName());
#else
	// And native files on other platforms. (ie. .ini on UNIX, XML on Mac)
	static QSettings cfg;
#endif

	while(cfg.group().isEmpty()==false)
		cfg.endGroup();

	return cfg;
}

int main(int argc, char *argv[]) {
	DrawPileApp app(argc,argv);

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
				url.setUserName(app.getSettings().value("history/username").toString());
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

