/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include <QApplication>
#include <QSettings>
#include <QUrl>
#include <QTabletEvent>
#include <QIcon>

#include "main.h"
#include "mainwindow.h"

#include "utils/whatismyip.h"

DrawPileApp::DrawPileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.sourceforge.net");
	setApplicationName("drawpile");

	// Make sure a user name is set
	QSettings cfg;
	cfg.beginGroup("history");
	if(cfg.contains("username") == false ||
			cfg.value("username").toString().isEmpty())
	{
#ifdef Q_OS_WIN
		QString defaultname = getenv("USERNAME");
#else
		QString defaultname = getenv("USER");
#endif

		cfg.setValue("username", defaultname);
	}
	setWindowIcon(QIcon::fromTheme("drawpile", QIcon(":icons/drawpile.png")));
}

/**
 * Handle tablet proximity events. When the eraser is brought near
 * the tablet surface, switch to eraser tool on all windows.
 * When the tip leaves the surface, switch back to whatever tool
 * we were using before.
 */
bool DrawPileApp::event(QEvent *e) {
	if(e->type() == QEvent::TabletEnterProximity || e->type() == QEvent::TabletLeaveProximity) {
		QTabletEvent *te = static_cast<QTabletEvent*>(e);
		if(te->pointerType()==QTabletEvent::Eraser)
			emit eraserNear(e->type() == QEvent::TabletEnterProximity);

		return true;
	}
	return QApplication::event(e);
}

void DrawPileApp::notifySettingsChanged()
{
	emit settingsChanged();
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
			// Join the session
			QUrl url(arg, QUrl::TolerantMode);
			if(url.userName().isEmpty()) {
				// Set username if not specified
				QSettings cfg;
				url.setUserName(cfg.value("history/username").toString());
			}
			win->joinSession(url);

		} else {
			win->open(argv[1]);
		}
	}

	return app.exec();
}

