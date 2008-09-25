/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include <QApplication>
#include <QSettings>
#include <QUrl>
#include <QMessageBox>
#include <QTabletEvent>
#include <QIcon>

#ifdef CRASHGUARD
	#include <QFile>
	#include <QFileInfo>
	#include <QDebug>
	#include <QDir>
#endif

#include "main.h"
#include "mainwindow.h"

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
	setWindowIcon(QIcon(":icons/drawpile.png"));
}

QSettings& DrawPileApp::getSettings()
{
#if 0 /*def Q_WS_WIN*/
	// QColor serialization seems to be broken currently when using IniFormat on windows
	// Use .ini files on windows
	static QSettings cfg(QSettings::IniFormat, QSettings::UserScope,
			organizationName(), applicationName());
#endif
	// And native files on other platforms. (ie. .ini on UNIX, XML on Mac)
	static QSettings cfg;

	while(cfg.group().isEmpty()==false)
		cfg.endGroup();

	return cfg;
}

/**
 * Get the location of configuration files.
 * @return path to configuration directory
 */
QString DrawPileApp::getConfDir()
{
	// TODO Location on MacOS
#ifdef WIN32
	return QString("%1\\DrawPile\\").arg(getenv("APPDATA"));
#else
	return QString("%1/.config/DrawPile/").arg(getenv("HOME"));
#endif
}

/**
 * Handle tablet proximity events. When the eraser is brought near
 * the tablet surface, switch to eraser tool on all windows.
 * When the tip leaves the surface, switch back to whatever tool
 * we were using before.
 */
bool DrawPileApp::event(QEvent *e) {
	if(e->type() == QEvent::TabletEnterProximity) {
		QTabletEvent *te = static_cast<QTabletEvent*>(e);
		if(te->pointerType()==QTabletEvent::Eraser) {
			foreach(QWidget *widget, topLevelWidgets()) {
				MainWindow *mw = qobject_cast<MainWindow*>(widget);
				if(mw)
					mw->eraserNear(true);
			}
			return true;
		}
	} else if(e->type() == QEvent::TabletLeaveProximity) {
		foreach(QWidget *widget, topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow*>(widget);
			if(mw)
				mw->eraserNear(false);
		}
		return true;
	}
	return QApplication::event(e);
}

int main(int argc, char *argv[]) {
	DrawPileApp app(argc,argv);

	// Create the main window
	MainWindow *win = new MainWindow;
	
	#ifdef CRASHGUARD
	QFile crashGuard(QFileInfo(DrawPileApp::getConfDir(), "crash.guard").absoluteFilePath());
	
	if (crashGuard.exists())
	{
		// crash recovery
		QFileInfoList autosaves = QDir(DrawPileApp::getConfDir()).entryInfoList(
			QStringList("*.png"),
			QDir::Files|QDir::Readable
			);
		
		/** @todo query user which of these should be opened, if any. */
		foreach(QFileInfo asav, autosaves)
		{
			win->open(asav.absoluteFilePath());
		}
	}
	
	if (!crashGuard.open(QIODevice::WriteOnly|QIODevice::Unbuffered))
		qWarning() << "crash guard creation failed!";
	#endif // CRASHGUARD
	
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
				url.setUserName(app.getSettings().value("history/username").toString());
			}
			win->joinSession(url);
		} else {
			if(win->initBoard(argv[1])==false) {
				// If image couldn't be loaded, initialize to a default board
				// and show error message.
				QMessageBox::warning(win, app.tr("DrawPile"),
						app.tr("Couldn't load image %1.").arg(argv[1]));
			}
		}
	}

	int rv = app.exec();
	
	#ifdef CRASHGUARD
	crashGuard.remove();
	#endif
	
	return rv;
}

