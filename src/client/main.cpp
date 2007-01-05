/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

#include "mainwindow.h"

int main(int argc, char *argv[]) {
	QApplication app(argc,argv);

	// These are used by QSettings
	app.setOrganizationName("DrawPile");
	app.setOrganizationDomain("drawpile.sourceforge.net");
	app.setApplicationName("DrawPile");

	// Create and show the main window
	MainWindow win;

	if(argc>1) {
		// A parameter was given. We assume it to be a filename
		if(win.initBoard(argv[1])==false) {
			std::cerr << argv[1] << ": couldn't load image.\n";
			return 1;
		}
	} else {
		// Create a default board
		win.initBoard(QSize(800,600), Qt::white);
	}
	win.show();

	return app.exec();
}

