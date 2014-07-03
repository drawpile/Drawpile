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

#include "config.h"

#include "main.h"
#include "mainwindow.h"

#include "utils/icon.h"

#include <QSettings>
#include <QUrl>
#include <QTabletEvent>
#include <QStandardPaths>
#include <QLibraryInfo>
#include <QTranslator>
#include <QDir>

DrawPileApp::DrawPileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.sourceforge.net");
	setApplicationName("drawpile");

	// Set resource search paths
	QStringList iconPaths;
#ifdef BUILTIN_ICONS
	iconPaths << ":/icons";
#else
	iconPaths << qApp->applicationDirPath() + "/icons";
	for(const QString path : QStandardPaths::standardLocations(QStandardPaths::DataLocation))
		iconPaths << path + "/icons";
#endif
	QDir::setSearchPaths("icons", iconPaths);

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
	setWindowIcon(icon::fromTheme("drawpile"));
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


void initTranslations(const QLocale &locale)
{
	// Qt's own translations
	QTranslator *qtTranslator = new QTranslator;
	qtTranslator->load(locale, "qt", "_", QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	qApp->installTranslator(qtTranslator);

	// Our translations
	QTranslator *myTranslator = new QTranslator;
	QStringList datapaths;
	datapaths << qApp->applicationDirPath();
	datapaths << QStandardPaths::standardLocations(QStandardPaths::DataLocation);
	for(const QString &datapath : datapaths) {
		if(myTranslator->load(locale, "drawpile", "_", datapath + "/i18n"))
			break;
	}

	if(myTranslator->isEmpty())
		delete myTranslator;
	else
		qApp->installTranslator(myTranslator);
}

int main(int argc, char *argv[]) {
	// Initialize application
	DrawPileApp app(argc,argv);

	initTranslations(QLocale::system());

	// Create the main window
	MainWindow *win = new MainWindow;
	
	const QStringList args = app.arguments();
	if(args.count()>1) {
		QUrl url = args.at(1);

		if(url.scheme() == "drawpile") {
			// Our own protocol: connect to a session

			if(url.userName().isEmpty()) {
				// Set username if not specified
				QSettings cfg;
				url.setUserName(cfg.value("history/username").toString());
			}
			win->joinSession(url);

		} else {
			// Other protocols: load image
			if(url.scheme().length() <= 1) {
				// unset or 1 letter long (drive letter) scheme means this is probably
				// a local filesystem path.
				url = QUrl::fromLocalFile(args.at(1));
			}

			win->open(url);
		}
	}

	return app.exec();
}
