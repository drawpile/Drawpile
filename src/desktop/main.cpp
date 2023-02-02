/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

#include "desktop/main.h"
#include "desktop/mainwindow.h"

#include "libclient/utils/icon.h"
#include "libclient/utils/logging.h"
#include "libclient/utils/colorscheme.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/notifications.h"
#include "desktop/dialogs/versioncheckdialog.h"
#include "libshared/util/qtcompat.h"
#include "libshared/util/paths.h"
#include "rustpile/rustpile.h"

#ifdef Q_OS_MACOS
#include "desktop/widgets/macmenu.h"
#endif

#if defined(Q_OS_WIN) && defined(KIS_TABLET)
#include "desktop/bundled/kis_tablet/kis_tablet_support_win8.h"
#include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#endif

#include <QCommandLineParser>
#include <QSettings>
#include <QUrl>
#include <QTabletEvent>
#include <QLibraryInfo>
#include <QTranslator>
#include <QDateTime>
#include <QStyle>

#include <QtColorWidgets/ColorWheel>

DrawpileApp::DrawpileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.net");
	setApplicationName("drawpile");
#ifdef BUILD_LABEL
	setApplicationVersion(DRAWPILE_VERSION " " BUILD_LABEL);
	setApplicationDisplayName("Drawpile (" BUILD_LABEL ")");
#else
	setApplicationVersion(DRAWPILE_VERSION);
	setApplicationDisplayName("Drawpile");
#endif
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
		if(te->pointerType()==compat::PointerType::Eraser)
			emit eraserNear(e->type() == QEvent::TabletEnterProximity);
		return true;

	} else if(e->type() == QEvent::FileOpen) {
		QFileOpenEvent *fe = static_cast<QFileOpenEvent*>(e);

		// Note. This is currently broken in Qt 5.3.1:
		// https://bugreports.qt-project.org/browse/QTBUG-39972
		openUrl(fe->url());

		return true;

	}
#ifdef Q_OS_MACOS
	else if(e->type() == QEvent::ApplicationStateChange) {
		QApplicationStateChangeEvent *ae = static_cast<QApplicationStateChangeEvent*>(e);
		if(ae->applicationState() == Qt::ApplicationActive && topLevelWindows().isEmpty()) {
			// Open a new window when application is activated and there are no windows.
			openBlankDocument();
		}
	}
#endif

	return QApplication::event(e);
}

void DrawpileApp::notifySettingsChanged()
{
	emit settingsChanged();
}

void DrawpileApp::setDarkTheme(bool dark)
{
	QPalette pal;
	if(dark) {
		const QString paletteFile = utils::paths::locateDataFile("nightmode.colors");
		if(paletteFile.isEmpty()) {
			qWarning("Cannot switch to night mode: couldn't find color scheme file!");
		} else {
			pal = colorscheme::loadFromFile(paletteFile);
		}

	} else {
		pal = style()->standardPalette();
	}

	setPalette(pal);
	icon::selectThemeVariant();
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
		win->join(url);

	} else {
		// Other protocols: load image
		win->open(url);
	}
}

void DrawpileApp::openBlankDocument()
{
	// Open a new window with a blank image
	QSettings cfg;

	const QSize maxSize {65536, 65536};
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

static void initTranslations(const QLocale &locale)
{
	const auto preferredLangs = locale.uiLanguages();
	if(preferredLangs.size()==0)
		return;

	// TODO we should work our way down the preferred language list
	// until we find one a translation exists for.
	QString preferredLang = preferredLangs.at(0);

	// On Windows, the locale name is sometimes in the form "fi-FI"
	// rather than "fi_FI" that Qt expects.
	preferredLang.replace('-', '_');

	// Special case: if english is preferred language, no translations are needed.
	if(preferredLang == "en")
		return;

	// Qt's own translations
	QTranslator *qtTranslator = new QTranslator;
	if(!qtTranslator->load("qt_" + preferredLang, compat::libraryPath(QLibraryInfo::TranslationsPath)))
		qWarning("Qt translations not found");
	qApp->installTranslator(qtTranslator);

	for(auto &bundle : { "libclient_", "drawpile_" }) {
		// Our translations
		QTranslator *myTranslator = new QTranslator;

		for(const QString &datapath : utils::paths::dataPaths()) {
			if(myTranslator->load(bundle + preferredLang, datapath + "/i18n"))
				break;
		}

		if(myTranslator->isEmpty())
			delete myTranslator;
		else
			qApp->installTranslator(myTranslator);
	}
}

// Initialize the application and return a list of files to be opened (if any)
static QStringList initApp(DrawpileApp &app)
{
	// Parse command line arguments
	QCommandLineParser parser;
	parser.addHelpOption();
	parser.addVersionOption();

	// --data-dir
	QCommandLineOption dataDir("data-dir", "Override read-only data directory.", "path");
	parser.addOption(dataDir);

	// --portable-data-dir
	QCommandLineOption portableDataDir("portable-data-dir", "Override settings directory.", "path");
	parser.addOption(portableDataDir);

	// URL
	parser.addPositionalArgument("url", "Filename or URL.");

	parser.process(app);

	// Override data directories
	if(parser.isSet(dataDir))
		utils::paths::setDataPath(parser.value(dataDir));

	if(parser.isSet(portableDataDir)) {
		utils::paths::setWritablePath(parser.value(portableDataDir));

		QSettings::setDefaultFormat(QSettings::IniFormat);
		QSettings::setPath(
			QSettings::IniFormat,
			QSettings::UserScope,
			utils::paths::writablePath(QStandardPaths::AppConfigLocation, QString())
		);
	}

	// Continue initialization (can use QSettings from now on)
	utils::initLogging();

	// Override widget theme
	const int theme = QSettings().value("settings/theme", 0).toInt();
	if(theme != 0) // choice 0: system theme
		app.setStyle("fusion");

	if(theme==2) // choice 2: dark theme
		app.setDarkTheme(true);
	else
		icon::selectThemeVariant();

#ifdef Q_OS_MACOS
	// Mac specific settings
	app.setAttribute(Qt::AA_DontShowIconsInMenus);
	app.setQuitOnLastWindowClosed(false);

	// Global menu bar that is shown when no windows are open
	MacMenu::instance();
#endif

#if defined(Q_OS_WIN) && defined(KIS_TABLET)
	{
		bool useWindowsInk = false;
		// Enable Windows Ink tablet event handler
		// This was taken directly from Krita
		if(QSettings().value("settings/input/windowsink", true).toBool()) {
			KisTabletSupportWin8 *penFilter = new KisTabletSupportWin8();
			if (penFilter->init()) {
				app.installNativeEventFilter(penFilter);
				useWindowsInk = true;
				qDebug("Using Win8 Pointer Input for tablet support");

			} else {
				qWarning("No Win8 Pointer Input available");
				delete penFilter;
			}
		} else {
			qDebug("Win8 Pointer Input disabled");
		}

		if(!useWindowsInk) {
			// Enable modified Wintab support
			// This too was taken from Krita
			qDebug("Enabling custom Wintab support");
			KisTabletSupportWin::init();
		}
	}
#endif

	// Set override locale from settings, or use system locale if no override is set
	QLocale locale = QLocale::c();
	QString overrideLang = QSettings().value("settings/language").toString();
	if(!overrideLang.isEmpty())
		locale = QLocale(overrideLang);

	if(locale == QLocale::c())
		locale = QLocale::system();

	initTranslations(locale);

	return parser.positionalArguments();
}

int main(int argc, char *argv[]) {
#ifndef HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
	// Set attributes that must be set before QApplication is constructed
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

	// CanvasView does not work correctly with this enabled.
	// (Scale factor must be taken in account when zooming)
	//QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	DrawpileApp app(argc, argv);

	{
		const auto files = initApp(app);

		rustpile::rustpile_init_logging(&utils::logMessage);

		if(files.isEmpty()) {
			app.openBlankDocument();

		} else {
			QUrl url(files.at(0));
			if(url.scheme().length() <= 1) {
				// no scheme (or a drive letter?) means this is probably a local file
				url = QUrl::fromLocalFile(files.at(0));
			}

			app.openUrl(url);
		}
	}

	dialogs::VersionCheckDialog::doVersionCheckIfNeeded();

	return app.exec();
}
