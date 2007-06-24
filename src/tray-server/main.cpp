/******************************************************************************

   Copyright (C) 2007 M.K.A. <wyrmchild@users.sourceforge.net>

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

******************************************************************************/

#include <QtGui>

#include "traymenu.h"
#include "../server/sockets.h"

int main(int argc, char **argv)
{
	#ifdef NEED_NET
	Net net;
	#endif
	
	//QT_REQUIRE_VERSION(argc, argv, "4.3")
	Q_INIT_RESOURCE(systray);
	
	QApplication app(argc, argv);
	app.setQuitOnLastWindowClosed(false);
	
	if (!QSystemTrayIcon::isSystemTrayAvailable())
	{
		QMessageBox::critical(0, "DrawPile Tray Server",
			QObject::tr("No systray detected!"));
		return 1;
	}
	
	TrayMenu menu;
	//win.show();
	
	return app.exec();
}

