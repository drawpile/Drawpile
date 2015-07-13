/*
   Drawpile - a collaborative drawing program.

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
#include "notifications.h"

#ifdef Q_OS_MAC
#include "widgets/macmenu.h"
#endif

#include <QSettings>
#include <QUrl>
#include <QTabletEvent>
#include <QStandardPaths>
#include <QLibraryInfo>
#include <QTranslator>
#include <QDir>
#include <QDesktopWidget>
#include <QDateTime>

#include <Color_Wheel>

DrawpileApp::DrawpileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.sourceforge.net");
	setApplicationName("drawpile");
	setApplicationDisplayName("Drawpile");

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
	setWindowIcon(QIcon(":/icons/drawpile.png"));
}

/**
 * Handle tablet proximity events. When the eraser is brought near
 * the tablet surface, switch to eraser tool on all windows.
 * When the tip leaves the surface, switch back to whatever tool
 * we were using before.
 *
 * Also, on MacOS we must also handle the Open File event.
 */
bool DrawpileApp::event(QEvent *e) {
	if(e->type() == QEvent::TabletEnterProximity || e->type() == QEvent::TabletLeaveProximity) {
		QTabletEvent *te = static_cast<QTabletEvent*>(e);
		if(te->pointerType()==QTabletEvent::Eraser)
			emit eraserNear(e->type() == QEvent::TabletEnterProximity);
		return true;

	} else if(e->type() == QEvent::FileOpen) {
		QFileOpenEvent *fe = static_cast<QFileOpenEvent*>(e);

		// Note. This is currently broken in Qt 5.3.1:
		// https://bugreports.qt-project.org/browse/QTBUG-39972
		openUrl(fe->url());

		return true;
	}

	return QApplication::event(e);
}

void DrawpileApp::notifySettingsChanged()
{
	emit settingsChanged();
}

void DrawpileApp::openUrl(QUrl url)
{
	// See if there is an existing replacable window
	MainWindow *win = nullptr;
	for(QWidget *widget : topLevelWidgets()) {
		MainWindow *mw = qobject_cast<MainWindow*>(widget);
		if(mw && mw->canReplace()) {
			win = mw;
			break;
		}
	}

	// No? Create a new one
	if(!win)
		win = new MainWindow;

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
		win->open(url);
	}
}

QStringList DrawpileApp::dataPaths()
{
	QStringList datapaths;
#ifndef Q_OS_MAC
	datapaths << qApp->applicationDirPath();
	datapaths << QStandardPaths::standardLocations(QStandardPaths::DataLocation);
#else
	datapaths << QDir(qApp->applicationDirPath() + QStringLiteral("/../Resources")).absolutePath();
#endif
	return datapaths;
}

void initTranslations(const QLocale &locale)
{
	QStringList preferredLangs = locale.uiLanguages();
	if(preferredLangs.size()==0)
		return;

	// uiLanguages sometimes returns more languages than
	// than the user has actually selected, so we just pick
	// the first one.
	QString preferredLang = preferredLangs.at(0);

	// On Windows, the locale name is sometimes in the form "fi-FI"
	// rather than "fi_FI" that Qt expects.
	preferredLang.replace('-', '_');

	// Special case: if english is preferred language, no translations are needed.
	if(preferredLang == "en")
		return;

	// Qt's own translations
	QTranslator *qtTranslator = new QTranslator;
	qtTranslator->load("qt_" + preferredLang, QLibraryInfo::location(QLibraryInfo::TranslationsPath));
	qApp->installTranslator(qtTranslator);

	// Our translations
	QTranslator *myTranslator = new QTranslator;

	for(const QString &datapath : DrawpileApp::dataPaths()) {
		if(myTranslator->load("drawpile_" + preferredLang,  datapath + "/i18n"))
			break;
	}

	if(myTranslator->isEmpty())
		delete myTranslator;
	else
		qApp->installTranslator(myTranslator);
}

int main(int argc, char *argv[]) {
	// Initialize application
	DrawpileApp app(argc,argv);

	icon::selectThemeVariant();

#ifdef Q_OS_MAC
	// Mac specific settings
	app.setAttribute(Qt::AA_DontShowIconsInMenus);
	app.setQuitOnLastWindowClosed(false);

	// Global menu bar that is shown when no windows are open
	MacMenu::instance();
#endif

	qsrand(QDateTime::currentMSecsSinceEpoch());

	Color_Wheel::setDefaultDisplayFlags(Color_Wheel::SHAPE_SQUARE | Color_Wheel::ANGLE_FIXED | Color_Wheel::COLOR_HSV);

	{
		// Set override locale from settings, or use system locale if no override is set
		QLocale locale = QLocale::c();
		QString overrideLang = QSettings().value("settings/language").toString();
		if(!overrideLang.isEmpty())
			locale = QLocale(overrideLang);

		if(locale == QLocale::c())
			locale = QLocale::system();

		initTranslations(locale);
	}

	const QStringList args = app.arguments();
	if(args.count()>1) {
		QUrl url(args.at(1));
		if(url.scheme().length() <= 1) {
			// no scheme (or a drive letter?) means this is probably a local file
			url = QUrl::fromLocalFile(args.at(1));
		}

		app.openUrl(url);
	} else {
		// No arguments, start with an empty document
		QSettings cfg;

		QSize maxSize = app.desktop()->screenGeometry().size();
		QSize size = cfg.value("history/newsize").toSize();
		if(size.width()<100 || size.height()<100) {
			// No previous size, or really small size
			size = QSize(800, 600);
		} else {
			// Make sure previous size is not ridiculously huge
			size = size.boundedTo(maxSize);
		}

		QColor color = cfg.value("history/newcolor").value<QColor>();
		if(!color.isValid())
			color = Qt::white;

		MainWindow *win = new MainWindow;
		win->newDocument(size, color);
	}

	return app.exec();
}
