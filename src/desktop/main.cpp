// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "desktop/tabletinput.h"

#include "libclient/utils/logging.h"
#include "libclient/utils/colorscheme.h"
#include "desktop/utils/hidedocktitlebarseventfilter.h"
#include "desktop/notifications.h"
#include "desktop/dialogs/versioncheckdialog.h"
#include "libshared/util/paths.h"
#include "libclient/drawdance/global.h"
#include "libshared/util/qtcompat.h"
#include "cmake-config/config.h"
#include "desktop/utils/qtguicompat.h"

#ifdef Q_OS_MACOS
#include "desktop/utils/macui.h"
#include "desktop/widgets/macmenu.h"
#include <QTimer>
#elif defined(Q_OS_WIN) && defined(KIS_TABLET)
#include "desktop/bundled/kis_tablet/kis_tablet_support_win8.h"
#include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#endif

#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QSettings>
#include <QStyle>
#include <QUrl>
#include <QTabletEvent>
#include <QLibraryInfo>
#include <QTranslator>
#include <QDateTime>
#include <QWidget>

#include <QtColorWidgets/ColorWheel>

#ifdef ENABLE_VERSION_CHECK
#	include "dialogs/versioncheckdialog.h"
#endif

DrawpileApp::DrawpileApp(int &argc, char **argv)
	: QApplication(argc, argv)
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.net");
	setApplicationName("drawpile");
	setApplicationVersion(cmake_config::version());
	setApplicationDisplayName("Drawpile");
	setWindowIcon(QIcon(":/icons/drawpile.png"));

	drawdance::initLogging();
	drawdance::initCpuSupport();
	drawdance::DrawContextPool::init();

	// Dockers are hard to drag around since their title bars are full of stuff.
	// This event filter allows for hiding the title bars by holding shift.
	HideDockTitleBarsEventFilter *filter = new HideDockTitleBarsEventFilter{this};
	installEventFilter(filter);
	connect(filter, &HideDockTitleBarsEventFilter::setDockTitleBarsHidden, this,
		&DrawpileApp::setDockTitleBarsHidden);
}

DrawpileApp::~DrawpileApp()
{
	drawdance::DrawContextPool::deinit();
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

	} else if (e->type() == QEvent::ApplicationPaletteChange) {
		updateThemeIcons();
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
	tabletinput::update(this, QSettings{});
	emit settingsChanged();
}

void DrawpileApp::setTheme(int theme)
{
	if(theme < THEME_SYSTEM || theme >= THEME_COUNT) {
		theme = THEME_DEFAULT;
	}

	switch(theme) {
	case THEME_SYSTEM:
	case THEME_SYSTEM_LIGHT:
	case THEME_SYSTEM_DARK:
#ifdef Q_OS_WIN
		// Empty string will use the Vista style, which is so incredibly ugly
		// that it looks outright broken. So we use the old Windows-95-esque
		// style instead, it looks old, but at least not totally busted.
		setStyle(QStringLiteral("Windows"));
#elif defined(Q_OS_MACOS)
		setStyle(new macui::MacViewStatusBarProxyStyle);
#else
		setStyle(QStringLiteral(""));
#endif
		break;
	default:
		setStyle(QStringLiteral("Fusion"));
		break;
	}

	switch(theme) {
	case THEME_SYSTEM_DARK:
#ifdef Q_OS_MACOS
		if (macui::setNativeAppearance(macui::Appearance::Dark)) {
			setPalette(style()->standardPalette());
			break;
		}
#endif
		[[fallthrough]];
	case THEME_FUSION_DARK:
		setPalette(loadPalette(QStringLiteral("nightmode.colors")));
		break;
	case THEME_SYSTEM_LIGHT:
#ifdef Q_OS_MACOS
	if (macui::setNativeAppearance(macui::Appearance::Light)) {
		setPalette(style()->standardPalette());
		break;
	}
#endif
		[[fallthrough]];
	case THEME_KRITA_BRIGHT:
		setPalette(loadPalette(QStringLiteral("kritabright.colors")));
		break;
	case THEME_KRITA_DARK:
		setPalette(loadPalette(QStringLiteral("kritadark.colors")));
		break;
	case THEME_KRITA_DARKER:
		setPalette(loadPalette(QStringLiteral("kritadarker.colors")));
		break;
	default:
#ifdef Q_OS_MACOS
		macui::setNativeAppearance(macui::Appearance::System);
#endif
		setPalette(style()->standardPalette());
	}

	// When the global style changes, `QApplication::setStyle` only sends
	// `QEvent::StyleChange` to widgets without the `Qt::WA_SetStyle` attribute.
	// For efficiency, all `GroupedToolButton` share the same global style, and
	// that style needs some signal to reset itself. Sending `StyleChange` to
	// the application when its own style changes seems like a reasonable
	// choice, since the alternatives are either to have each widget have its
	// own copy of the proxy style and reset all of them every time, or have
	// the widgets notify the global object when they receive a `StyleChange`
	// event and then have to debounce all of them and guard against recursive
	// style changes.
	QEvent event(QEvent::StyleChange);
	QCoreApplication::sendEvent(this, &event);
}

QPalette DrawpileApp::loadPalette(const QString &fileName)
{
	QString path = utils::paths::locateDataFile(fileName);
	if(path.isEmpty()) {
		qWarning("Could not find palette file %s", qUtf8Printable(fileName));
		return style()->standardPalette();
	} else {
		return colorscheme::loadFromFile(path);
	}
}

void DrawpileApp::updateThemeIcons()
{
	const auto *iconTheme = QPalette().color(QPalette::Window).lightness() < 128
		? "dark"
		: "light";

	QStringList fallbackIconPaths;
	for (const auto &path : utils::paths::dataPaths()) {
		fallbackIconPaths.append(path + "/theme/" + iconTheme);
	}

	QDir::setSearchPaths("theme", fallbackIconPaths);
	QIcon::setThemeName(iconTheme);
}

void DrawpileApp::initTheme()
{
	static QStringList defaultThemePaths{QIcon::themeSearchPaths()};

	QStringList themePaths{defaultThemePaths};
	for (const auto &path : utils::paths::dataPaths()) {
		themePaths.append(path + "/theme");
	}
	QIcon::setThemeSearchPaths(themePaths);

	QSettings settings;

	// Override widget theme
	int theme = settings.value("settings/theme", DrawpileApp::THEME_SYSTEM).toInt();

	// System themes tend to look ugly and broken. Reset the theme once to get
	// a sensible default, the user can still revert to the ugly theme manually.
	int themeVersion = settings.value("settings/themeversion", 0).toInt();
	if(themeVersion < 1) {
		theme = DrawpileApp::THEME_DEFAULT;
		settings.setValue("settings/theme", theme);
		settings.setValue("settings/themeversion", DrawpileApp::THEME_VERSION);
	}

	setTheme(theme);
	updateThemeIcons();
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

void DrawpileApp::deleteAllMainWindowsExcept(MainWindow *win)
{
	for(QWidget *widget : topLevelWidgets()) {
		MainWindow *mw = qobject_cast<MainWindow *>(widget);
		if(mw && mw != win) {
			mw->deleteLater();
		}
	}
}

static void initTranslations(DrawpileApp &app, const QLocale &locale)
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
	// TODO: This is wrong since English still needs translations for N-variants
	if(preferredLang.mid(0, 2) == "en")
		return;

	auto *translator = new QTranslator(&app);
	for(const QString &datapath : utils::paths::dataPaths()) {
		if(translator->load("all_" + preferredLang, datapath + "/i18n"))
			break;
	}

	if(translator->isEmpty())
		delete translator;
	else
		qApp->installTranslator(translator);
}

#ifdef Q_OS_WIN
static bool shouldCopyNativeSettings(const QSettings &settings, const QString mode)
{
	if(mode.compare("true", Qt::CaseInsensitive) == 0) {
		return true;
	} else if(mode.compare("false", Qt::CaseInsensitive) == 0) {
		return false;
	} else if(mode.compare("if-empty", Qt::CaseInsensitive) == 0) {
		return settings.allKeys().isEmpty();
	} else {
		qCritical("Unknown --copy-legacy-settings value '%s'", qUtf8Printable(mode));
		std::exit(EXIT_FAILURE);
	}
}

static void copyNativeSettings(DrawpileApp &app, QSettings &settings)
{
	QSettings nativeSettings{
		QSettings::NativeFormat, QSettings::UserScope,
		app.organizationName(), app.applicationName()};
	for(const QString &key : nativeSettings.allKeys()) {
		settings.setValue(key, nativeSettings.value(key));
	}
}
#endif

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

	// --copy-legacy-settings
	QCommandLineOption copyLegacySettings("copy-legacy-settings",
		"Copy settings from Drawpile 2.1 to the new ini format. Use 'true' to "
		"copy the settings, 'false' to not copy them and 'if-empty' to only "
		"copy if the new settings are empty (this is the default.)",
		"mode", "if-empty");
	parser.addOption(copyLegacySettings);

	// URL
	parser.addPositionalArgument("url", "Filename or URL.");

	parser.process(app);

	// Override data directories
	if(parser.isSet(dataDir))
		utils::paths::setDataPath(parser.value(dataDir));

	if(parser.isSet(portableDataDir)) {
		utils::paths::setWritablePath(parser.value(portableDataDir));
		QSettings::setPath(
			QSettings::IniFormat,
			QSettings::UserScope,
			utils::paths::writablePath(QStandardPaths::AppConfigLocation, QString())
		);
	}
#ifdef Q_OS_WIN
	else {
		// It is easiest to get a reference to the native settings by creating
		// and holding onto a QSettings object before switching the default
		// format to INI
		QSettings settings;

		// On Windows, QSettings defaults to using the registry, which is awful.
		// We always want to use config files, so force that format.
		// This should never be used on other platforms since it causes the
		// settings to end up with non-standard location or file extension.
		QSettings::setDefaultFormat(QSettings::IniFormat);
		if(shouldCopyNativeSettings(settings, parser.value(copyLegacySettings))) {
			// Port over the settings from Drawpile 2.1 so that the user doesn't
			// have to set them up again just because our format changed.
			copyNativeSettings(app, settings);
		}
	}
#endif

	// Continue initialization (can use QSettings from now on)
	utils::initLogging();

	QSettings settings;

	app.initTheme();

#ifdef Q_OS_MACOS
	// Mac specific settings
	app.setAttribute(Qt::AA_DontShowIconsInMenus);
	app.setQuitOnLastWindowClosed(false);

	// Global menu bar that is shown when no windows are open
	MacMenu::instance();

	// This is a hack to deal with the menu disappearing when the final
	// window is closed by a confirmation sheet.
	QObject::connect(&app, &QGuiApplication::lastWindowClosed, [] {
		QTimer::singleShot(0, [] {
			qGuiApp->focusWindowChanged(nullptr);
		});
	});
#endif

	tabletinput::update(&app, settings);

	// Set override locale from settings, or use system locale if no override is set
	QLocale locale = QLocale::c();
	QString overrideLang = settings.value("settings/language").toString();
	if(!overrideLang.isEmpty())
		locale = QLocale(overrideLang);

	if(locale == QLocale::c())
		locale = QLocale::system();

	initTranslations(app, locale);

	return parser.positionalArguments();
}

int main(int argc, char *argv[]) {
#ifndef HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
	// Set attributes that must be set before QApplication is constructed
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#ifdef Q_OS_ANDROID
	// Android has a weird title bar by default, we want a menu bar instead.
	QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif

	// CanvasView does not work correctly with this enabled.
	// (Scale factor must be taken in account when zooming)
	//QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

	DrawpileApp app(argc, argv);

	{
		const auto files = initApp(app);

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

#ifdef ENABLE_VERSION_CHECK
	dialogs::VersionCheckDialog::doVersionCheckIfNeeded();
#endif

	return app.exec();
}
