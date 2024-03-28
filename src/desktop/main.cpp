// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/main.h"
#include "cmake-config/config.h"
#include "desktop/dialogs/startdialog.h"
#include "desktop/mainwindow.h"
#include "desktop/notifications.h"
#include "desktop/settings.h"
#include "desktop/tabletinput.h"
#include "desktop/utils/globalkeyeventfilter.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/recents.h"
#include "libclient/drawdance/global.h"
#include "libclient/utils/colorscheme.h"
#include "libclient/utils/logging.h"
#include "libclient/utils/statedatabase.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QMetaEnum>
#include <QScreen>
#include <QStyle>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTabletEvent>
#include <QTranslator>
#include <QUrl>
#include <QWidget>
#include <memory>
#include <tuple>
#ifdef Q_OS_MACOS
#	include "desktop/utils/macui.h"
#	include "desktop/widgets/macmenu.h"
#	include <QTimer>
#elif defined(Q_OS_WIN)
#	include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#	include "desktop/bundled/kis_tablet/kis_tablet_support_win8.h"
#elif defined(Q_OS_ANDROID)
#	include "libshared/util/androidutils.h"
#endif

DrawpileApp::DrawpileApp(int &argc, char **argv)
	: QApplication(argc, argv)
	, m_notifications(new notification::Notifications(this))
	, m_originalSystemStyle{compat::styleName(*style())}
	, m_originalSystemPalette{style()->standardPalette()}
{
	setOrganizationName("drawpile");
	setOrganizationDomain("drawpile.net");
	setApplicationName("drawpile");
	setApplicationVersion(cmake_config::version());
	setApplicationDisplayName("Drawpile");
	setWindowIcon(QIcon(":/icons/drawpile.png"));
	// QSettings has trouble loading types enums if they haven't been
	// instantiated before, so we just instantiate every default value once to
	// make sure they all exist before loading settings.
	desktop::settings::initializeTypes();
	// QSettings will use the wrong settings when it is opened before all
	// the app and organisation names are set.
	m_settings.reset();

	drawdance::initLogging();
	drawdance::initCpuSupport();
	drawdance::DrawContextPool::init();

	// Dockers are hard to drag around since their title bars are full of stuff.
	// This event filter allows for hiding the title bars by holding shift.
	GlobalKeyEventFilter *filter = new GlobalKeyEventFilter{this};
	installEventFilter(filter);
	connect(
		filter, &GlobalKeyEventFilter::setDockTitleBarsHidden, this,
		&DrawpileApp::setDockTitleBarsHidden);
	connect(
		filter, &GlobalKeyEventFilter::focusCanvas, this,
		&DrawpileApp::focusCanvas);
#ifdef Q_OS_WIN
	installNativeEventFilter(&winEventFilter);
#endif
}

DrawpileApp::~DrawpileApp()
{
	drawdance::DrawContextPool::deinit();
}

void DrawpileApp::initState()
{
	Q_ASSERT(!m_state);
	Q_ASSERT(!m_recents);
	m_state = new utils::StateDatabase{this};
	m_recents = new utils::Recents{*m_state};
}

/**
 * Handle tablet proximity events. When the eraser is brought near
 * the tablet surface, switch to eraser tool on all windows.
 * When the tip leaves the surface, switch back to whatever tool
 * we were using before. The browser does not report proximity events.
 *
 * Also, on MacOS we must also handle the Open File event.
 */
bool DrawpileApp::event(QEvent *e)
{
	switch(e->type()) {
#ifndef __EMSCRIPTEN__
	case QEvent::TabletEnterProximity: {
		QTabletEvent *te = static_cast<QTabletEvent *>(e);
		bool eraser = te->pointerType() == compat::PointerType::Eraser;
		emit tabletProximityChanged(true, eraser);
		updateEraserNear(eraser);
		return true;
	}
	case QEvent::TabletLeaveProximity: {
		QTabletEvent *te = static_cast<QTabletEvent *>(e);
		bool eraser = te->pointerType() == compat::PointerType::Eraser;
		emit tabletProximityChanged(false, eraser);
		if(eraser) {
			updateEraserNear(false);
		}
		return true;
	}
#endif

	case QEvent::FileOpen: {
		QFileOpenEvent *fe = static_cast<QFileOpenEvent *>(e);
		openPath(fe->file());
		return true;
	}

	case QEvent::ApplicationPaletteChange:
		updateThemeIcons();
		break;

#ifdef Q_OS_MACOS
	case QEvent::ApplicationStateChange: {
		QApplicationStateChangeEvent *ae =
			static_cast<QApplicationStateChangeEvent *>(e);
		if(ae->applicationState() == Qt::ApplicationActive &&
		   topLevelWindows().isEmpty()) {
			// Open a new window when application is activated and there are no
			// windows.
			openStart();
		}
	}
#endif

	default:
		break;
	}

	return QApplication::event(e);
}

#ifndef __EMSCRIPTEN__
void DrawpileApp::updateEraserNear(bool near)
{
	if(near != m_wasEraserNear) {
		m_wasEraserNear = near;
		emit eraserNear(near);
	}
}
#endif

void DrawpileApp::setThemeStyle(const QString &themeStyle)
{
	bool foundStyle = false;
#ifdef Q_OS_MACOS
	if(themeStyle.isEmpty() || themeStyle.startsWith(QStringLiteral("mac"))) {
		foundStyle = true;
		setStyle(new macui::MacProxyStyle);
	} else {
		foundStyle = setStyle(themeStyle);
	}
#else
	foundStyle =
		setStyle(themeStyle.isEmpty() ? m_originalSystemStyle : themeStyle);
#endif

	if(!foundStyle) {
		qWarning() << "Could not find style" << themeStyle;
		return;
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

void DrawpileApp::setThemePalette(desktop::settings::ThemePalette themePalette)
{
	using desktop::settings::ThemePalette;
	QPalette newPalette;

	switch(themePalette) {
	case ThemePalette::System:
#ifdef Q_OS_MACOS
		macui::setNativeAppearance(macui::Appearance::System);
#endif
		setPalette(m_originalSystemPalette);
		return;
	case ThemePalette::Dark:
#ifdef Q_OS_MACOS
		if(macui::setNativeAppearance(macui::Appearance::Dark)) {
			setPalette(QPalette());
			return;
		}
#endif
		newPalette = loadPalette(QStringLiteral("nightmode.colors"));
		break;
	case ThemePalette::Light:
#ifdef Q_OS_MACOS
		if(macui::setNativeAppearance(macui::Appearance::Light)) {
			setPalette(QPalette());
			return;
		}
#endif
		[[fallthrough]];
	case ThemePalette::KritaBright:
		newPalette = loadPalette(QStringLiteral("kritabright.colors"));
		break;
	case ThemePalette::KritaDark:
		newPalette = loadPalette(QStringLiteral("kritadark.colors"));
		break;
	case ThemePalette::KritaDarker:
		newPalette = loadPalette(QStringLiteral("kritadarker.colors"));
		break;
	case ThemePalette::Fusion: {
#ifdef Q_OS_MACOS
		// Fusion palette will adapt itself to whether the system appearance is
		// light or dark, so we need to reset that or else it will incorrectly
		// inherit the dark mode of the previous palette
		macui::setNativeAppearance(macui::Appearance::System);
#endif
		if(compat::styleName(*style()) == QStringLiteral("Fusion")) {
			newPalette = style()->standardPalette();
		} else if(
			const auto fusion =
				std::unique_ptr<QStyle>(QStyleFactory::create("Fusion"))) {
			newPalette = fusion->standardPalette();
		}
		break;
	}
	case ThemePalette::HotdogStand:
		newPalette = loadPalette(QStringLiteral("hotdogstand.colors"));
		break;
	}

	setPalette(newPalette);
#ifdef Q_OS_MACOS
	// If the macOS theme is used with custom palettes, it is necessary to
	// adjust the native appearance to match since some controls (e.g. combobox)
	// continue to draw using the native appearance for background and only
	// adopt the palette for parts of the UI.
	if(newPalette.color(QPalette::Window).lightness() < 128) {
		macui::setNativeAppearance(macui::Appearance::Dark);
	} else {
		macui::setNativeAppearance(macui::Appearance::Light);
	}
#endif
}

QPalette DrawpileApp::loadPalette(const QString &fileName)
{
	QString path = utils::paths::locateDataFile(fileName);
	if(path.isEmpty()) {
		qWarning("Could not find palette file %s", qUtf8Printable(fileName));
		return QPalette();
	} else {
		return colorscheme::loadFromFile(path);
	}
}

void DrawpileApp::updateThemeIcons()
{
	const auto *iconTheme =
		QPalette().color(QPalette::Window).lightness() < 128 ? "dark" : "light";

	QStringList fallbackIconPaths;
	for(const auto &path : utils::paths::dataPaths()) {
		fallbackIconPaths.append(path + "/theme/" + iconTheme);
	}

	QDir::setSearchPaths("theme", fallbackIconPaths);
	QIcon::setThemeName(iconTheme);
}

void DrawpileApp::initTheme()
{
	static QStringList defaultThemePaths{QIcon::themeSearchPaths()};

	QStringList themePaths;
	for(const auto &path : utils::paths::dataPaths()) {
		themePaths.append(path + "/theme");
	}
	themePaths.append(defaultThemePaths);
	QIcon::setThemeSearchPaths(themePaths);

	m_settings.bindThemeStyle(this, &DrawpileApp::setThemeStyle);
	m_settings.bindThemePalette(this, &DrawpileApp::setThemePalette);
	// If the theme is set to the default theme then the palette will not change
	// and icon initialisation will be incomplete if it is not updated once here
	updateThemeIcons();
}

void DrawpileApp::initCanvasImplementation(const QString &arg)
{
	using desktop::settings::CanvasImplementation;
	int canvasImplementation;
	if(QStringLiteral("system").compare(arg, Qt::CaseInsensitive) == 0) {
		canvasImplementation = int(CanvasImplementation::Default);
	} else if(
		QStringLiteral("qgraphicsview").compare(arg, Qt::CaseInsensitive) ==
		0) {
		canvasImplementation = int(CanvasImplementation::GraphicsView);
	} else if(QStringLiteral("opengl").compare(arg, Qt::CaseInsensitive) == 0) {
		canvasImplementation = int(CanvasImplementation::OpenGl);
	} else {
		if(QStringLiteral("none").compare(arg, Qt::CaseInsensitive) != 0) {
			qWarning("Unknown --renderer '%s'", qUtf8Printable(arg));
		}
		canvasImplementation = m_settings.renderCanvas();
	}
	m_canvasImplementation = getCanvasImplementationFor(canvasImplementation);
}

void DrawpileApp::initInterface()
{
	if(m_settings.interfaceMode() ==
	   int(desktop::settings::InterfaceMode::Unknown)) {
		m_settings.setInterfaceMode(int(guessInterfaceMode()));
	}
	m_smallScreenMode = m_settings.interfaceMode() ==
						int(desktop::settings::InterfaceMode::SmallScreen);

	QFont font = QApplication::font();
	int fontSize = m_settings.fontSize();
	if(fontSize <= 0) {
		// We require a point size. Android uses a pixel size, which causes the
		// point size to be reported as -1 and breaks several UI elements. But
		// the font size is too ginormous there to be usable anyway, so it makes
		// sense to force it to a different value anyway. The font in the
		// browser tends to be too large as well, force it too.
#ifdef __EMSCRIPTEN__
		fontSize = 10;
#else
		int pointSize = font.pointSize();
		fontSize = pointSize <= 0 ? 9 : pointSize;
#endif
		m_settings.setFontSize(fontSize);
	}

	if(m_settings.overrideFontSize()) {
		font.setPointSize(fontSize);
		QApplication::setFont(font);
	}
}

desktop::settings::InterfaceMode DrawpileApp::guessInterfaceMode()
{
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	auto [pixelSize, mmSize] = screenResolution();
	if(pixelSize.width() < 700 || pixelSize.height() < 700 ||
	   mmSize.width() < 80.0 || mmSize.height() < 80.0) {
		return desktop::settings::InterfaceMode::SmallScreen;
	}
#endif
	return desktop::settings::InterfaceMode::Desktop;
}

bool DrawpileApp::canvasImplementationUsesTileCache()
{
	using desktop::settings::CanvasImplementation;
	return canvasImplementation() != int(CanvasImplementation::GraphicsView);
}

int DrawpileApp::getCanvasImplementationFor(int canvasImplementation)
{
	using desktop::settings::CanvasImplementation;
	switch(canvasImplementation) {
	case int(CanvasImplementation::GraphicsView):
	case int(CanvasImplementation::OpenGl):
		return canvasImplementation;
	default:
#ifdef __EMSCRIPTEN__
		return int(CanvasImplementation::OpenGl);
#else
		return int(CanvasImplementation::GraphicsView);
#endif
	}
}

QPair<QSize, QSizeF> DrawpileApp::screenResolution()
{
	QScreen *screen = primaryScreen();
	if(screen) {
		return {screen->availableVirtualSize(), screen->physicalSize()};
	} else {
		return {{0, 0}, {0.0, 0.0}};
	}
}

MainWindow *DrawpileApp::acquireWindow(bool singleSession)
{
	if(singleSession) {
		return new MainWindow(true, singleSession);
	} else {
		for(QWidget *widget : topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw && mw->canReplace()) {
				return mw;
			}
		}
		return new MainWindow;
	}
}

void DrawpileApp::openPath(const QString &path)
{
	acquireWindow(false)->openPath(path);
}

void DrawpileApp::joinUrl(const QUrl &url, bool singleSession)
{
	acquireWindow(singleSession)->autoJoin(url);
}

dialogs::StartDialog::Entry getStartDialogEntry(const QString &page)
{
	if(page.isEmpty() || page.compare("guess", Qt::CaseInsensitive) != 0) {
		QMetaEnum entries = QMetaEnum::fromType<dialogs::StartDialog::Entry>();
		int count = entries.keyCount();
		for(int i = 0; i < count; ++i) {
			int value = entries.value(i);
			bool pageFound =
				value != dialogs::StartDialog::Entry::Count &&
				page.compare(entries.key(i), Qt::CaseInsensitive) == 0;
			if(pageFound) {
				return dialogs::StartDialog::Entry(value);
			}
		}
		qWarning("Unknown start-page '%s'", qUtf8Printable(page));
	}
	return dialogs::StartDialog::Entry::Guess;
}

void DrawpileApp::openStart(const QString &page)
{
	MainWindow *win = new MainWindow;
	win->newDocument(
		m_settings.newCanvasSize(), m_settings.newCanvasBackColor());
	if(page.compare("none", Qt::CaseInsensitive) != 0) {
		dialogs::StartDialog *dlg = win->showStartDialog();
		dlg->showPage(getStartDialogEntry(page));
	}
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

static QStringList gatherPotentialLanguages(const QLocale &locale)
{
	QStringList langs;
	for(const QString &lang : locale.uiLanguages()) {
		if(!langs.contains(lang)) {
			langs.append(lang);
		}
		// On some platforms, languages come with dashes instead of underscores,
		// so it's fi-FI instead of fi_FI like we want them to come as.
		QString langWithUnderscores = lang;
		langWithUnderscores.replace("-", "_");
		if(!langs.contains(langWithUnderscores)) {
			langs.append(langWithUnderscores);
		}
	}
	return langs;
}

static void initTranslations(DrawpileApp &app, const QLocale &locale)
{
	QTranslator *translator = new QTranslator{&app};
	for(const QString &lang : gatherPotentialLanguages(locale)) {
		QString filename = QStringLiteral("all_%1").arg(lang);
		for(const QString &datapath : utils::paths::dataPaths()) {
			QString directory = QStringLiteral("%1/i18n").arg(datapath);
			if(translator->load(filename, directory) &&
			   !translator->isEmpty()) {
				qApp->installTranslator(translator);
				return;
			}
		}
	}
	delete translator;
}

static QStringList getStartPages()
{
	QMetaEnum entries = QMetaEnum::fromType<dialogs::StartDialog::Entry>();
	int count = entries.keyCount();
	QStringList pages;
	for(int i = 0; i < count; ++i) {
		if(entries.value(i) != dialogs::StartDialog::Entry::Count) {
			pages.append(QString::fromUtf8(entries.key(i)).toLower());
		}
	}
	return pages;
}

// Initialize the application and return a list of files to be opened (if any)
static std::tuple<QStringList, QString, bool> initApp(DrawpileApp &app)
{
	// Parse command line arguments
	QCommandLineParser parser;
	parser.addHelpOption();
	parser.addVersionOption();

	// --data-dir
	QCommandLineOption dataDir(
		"data-dir", "Override read-only data directory.", "path");
	parser.addOption(dataDir);

	// --portable-data-dir
	QCommandLineOption portableDataDir(
		"portable-data-dir", "Override settings directory.", "path");
	parser.addOption(portableDataDir);

	// --opengl
	QCommandLineOption opengl(
		"opengl",
		"Set OpenGL implementation, overrides QT_OPENGL environment variable",
		"impl");
	parser.addOption(opengl);

#ifdef Q_OS_WIN
	// --copy-legacy-settings
	QCommandLineOption copyLegacySettings(
		"copy-legacy-settings",
		"Copy settings from Drawpile 2.1 to the new ini format. Use 'true' to "
		"copy the settings, 'false' to not copy them and 'if-empty' to only "
		"copy if the new settings are empty (this is the default.)",
		"mode", "if-empty");
	parser.addOption(copyLegacySettings);
#endif

	QString startPageDescription =
		QStringLiteral("Which page to show on the start dialog: guess (the "
					   "default), %1 or none.")
			.arg(getStartPages().join(", "));
	QCommandLineOption startPage(
		"start-page", startPageDescription, "page", "guess");
	parser.addOption(startPage);

	QCommandLineOption singleSession(
		"single-session",
		QStringLiteral("Run in single-session mode, allowing only the joining "
					   "of the given session. Intended for the browser version "
					   "of Drawpile."));
	parser.addOption(singleSession);

	QCommandLineOption renderer(
		"renderer",
		"Override canvas renderer, one of: none (don't override, the default), "
		"system (override with system default), qgraphicsview or opengl.",
		"renderer", "none");
	parser.addOption(renderer);

	// URL
	parser.addPositionalArgument("url", "Filename or URL.");

	parser.process(app);

	// Override data directories
	if(parser.isSet(dataDir))
		utils::paths::setDataPath(parser.value(dataDir));

	if(parser.isSet(portableDataDir)) {
		utils::paths::setWritablePath(parser.value(portableDataDir));
		app.settings().reset(utils::paths::writablePath(
			QStandardPaths::AppConfigLocation, "drawpile.ini"));
	}
#ifdef Q_OS_WIN
	else {
		const auto mode = parser.value(copyLegacySettings);
		if(mode.compare("true", Qt::CaseInsensitive) == 0) {
			app.settings().migrateFromNativeFormat(true);
		} else if(mode.compare("if-empty", Qt::CaseInsensitive) == 0) {
			app.settings().migrateFromNativeFormat(false);
		} else if(mode.compare("false", Qt::CaseInsensitive) != 0) {
			qCritical(
				"Unknown --copy-legacy-settings value '%s'",
				qUtf8Printable(mode));
			std::exit(EXIT_FAILURE);
		}
	}
#endif

	app.initState();
	desktop::settings::Settings &settings = app.settings();
	settings.bindWriteLogFile(&utils::enableLogFile);
	app.initTheme();
	app.initCanvasImplementation(parser.value(renderer));
	app.initInterface();

#ifdef Q_OS_ANDROID
	if(!settings.androidStylusChecked()) {
		// We'll flush the flag that we checked the input here in case we crash.
		settings.setAndroidStylusChecked(true);
		settings.trySubmit();
		// Enable fingerpainting if this device doesn't have a stylus.
		bool hasStylus = utils::androidHasStylusInput();
		settings.setOneFingerScroll(hasStylus);
		settings.setOneFingerDraw(!hasStylus);
	}

	settings.bindCaptureVolumeRocker([](bool capture) {
		if(capture) {
			qputenv("QT_ANDROID_VOLUME_KEYS", "1");
		} else {
			qunsetenv("QT_ANDROID_VOLUME_KEYS");
		}
	});
#endif

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

	tabletinput::init(app);
	parentalcontrols::init(settings);

	// Set override locale from settings, or use system locale if no override is
	// set
	QLocale locale = QLocale::c();
	QString overrideLang = settings.language();
	if(!overrideLang.isEmpty())
		locale = QLocale(overrideLang);

	if(locale == QLocale::c())
		locale = QLocale::system();

	initTranslations(app, locale);

	return {
		parser.positionalArguments(), parser.value(startPage),
		parser.isSet(singleSession)};
}

#ifdef __EMSCRIPTEN__
extern "C" int drawpileShouldPreventUnload()
{
	QApplication *app = qApp;
	if(app) {
		for(QWidget *widget : app->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw && mw->shouldPreventUnload()) {
				return true;
			}
		}
	}
	return false;
}
#endif

static int applyRenderSettingsFrom(const QString &path)
{
	QSettings cfg(path, QSettings::IniFormat);

#ifndef HAVE_QT_COMPAT_DEFAULT_HIGHDPI_SCALING
	QString enabledKey = QStringLiteral("enabled");
	if(cfg.contains(enabledKey)) {
		bool enabled = cfg.value(enabledKey).toBool();
		QApplication::setAttribute(Qt::AA_EnableHighDpiScaling, enabled);
	}
#endif

	if(qgetenv("QT_SCALE_FACTOR").isEmpty()) {
		if(cfg.value(QStringLiteral("override")).toBool()) {
			qreal factor = qBound(
				1.0, cfg.value(QStringLiteral("factor")).toInt() / 100.0, 4.0);
			qputenv("QT_SCALE_FACTOR", qUtf8Printable(QString::number(factor)));
		}
	}

	bool vsyncOk;
	int vsync = qgetenv("DRAWPILE_VSYNC").toInt(&vsyncOk);
	if(!vsyncOk) {
		vsync = cfg.value(QStringLiteral("vsync")).toInt();
	}
	return vsync;
}

static int applyRenderSettings(int argc, char **argv)
{
	QString fileName = QStringLiteral("scaling.ini");
	bool applied = false;
	int vsync = 0;
	for(int i = 1; i < argc - 1; ++i) {
		if(!applied && strcmp(argv[i], "--portable-data-dir") == 0) {
			QString path =
				QDir(QString::fromUtf8(argv[++i]) + QStringLiteral("/settings"))
					.absoluteFilePath(fileName);
			vsync = applyRenderSettingsFrom(path);
			applied = true;
		} else if(strcmp(argv[i], "--opengl") == 0) {
			qputenv("QT_OPENGL", argv[++i]);
		}
	}
	if(!applied) {
		// Can't use AppConfigLocation because the application name is not
		// initialized yet and doing so at this point corrupts the main settings
		// file. QSettings is terrible and we should really do away with it.
		vsync = applyRenderSettingsFrom(
			QStandardPaths::writableLocation(
				QStandardPaths::GenericConfigLocation) +
			QStringLiteral("/drawpile/") + fileName);
	}
	return vsync;
}

int main(int argc, char *argv[])
{
#ifndef HAVE_QT_COMPAT_DEFAULT_HIGHDPI_PIXMAPS
	// Set attributes that must be set before QApplication is constructed
	QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif

#ifndef HAVE_QT_COMPAT_DEFAULT_DISABLE_WINDOW_CONTEXT_HELP_BUTTON
	// Disable the "?" button in dialogs on Windows, we don't use those.
	QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

#ifdef Q_OS_ANDROID
	// Android has a weird title bar by default, we want a menu bar instead.
	QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar);
#endif

	// Don't compress input events, that causes jaggy lines on slow devices.
	QApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, false);
	QApplication::setAttribute(Qt::AA_CompressTabletEvents, false);

	int vsync = applyRenderSettings(argc, argv);
	// OpenGL rendering format. 8 bit color, no alpha, no depth. Stencil buffer
	// is needed for QPainter, without it, it can't manage to draw large paths
	// and spams the warning log with "Painter path exceeds +/-32767 pixels."
	QSurfaceFormat format = QSurfaceFormat::defaultFormat();
	if(vsync >= 0) {
		format.setSwapInterval(vsync);
	}
	format.setRedBufferSize(8);
	format.setGreenBufferSize(8);
	format.setBlueBufferSize(8);
	format.setAlphaBufferSize(0);
	format.setDepthBufferSize(0);
	format.setStencilBufferSize(8);
	QSurfaceFormat::setDefaultFormat(format);

#ifdef __EMSCRIPTEN__
	DrawpileApp *app = new DrawpileApp(argc, argv);
#else
	DrawpileApp appInstance(argc, argv);
	DrawpileApp *app = &appInstance;
#endif

	{
		const auto [files, page, singleSession] = initApp(*app);
		if(files.isEmpty()) {
			app->openStart(page);
		} else {
			const QString &arg = files.at(0);
			bool looksLikeSessionUrl =
				arg.startsWith("drawpile://", Qt::CaseInsensitive) ||
				arg.startsWith("ws://", Qt::CaseInsensitive) ||
				arg.startsWith("wss://", Qt::CaseInsensitive);
			if(looksLikeSessionUrl) {
				app->joinUrl(QUrl(arg), singleSession);
			} else {
				app->openPath(arg);
			}
		}
	}

#ifdef __EMSCRIPTEN__
	return 0;
#else
	return app->exec();
#endif
}
