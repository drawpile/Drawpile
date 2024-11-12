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
#include "dpcommon/platform_qt.h"
#include "libclient/brushes/brushpresetmodel.h"
#include "libclient/drawdance/global.h"
#include "libclient/utils/colorscheme.h"
#include "libclient/utils/logging.h"
#include "libclient/utils/statedatabase.h"
#include "libshared/util/paths.h"
#include <QCommandLineParser>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QIcon>
#include <QLibraryInfo>
#include <QMetaEnum>
#include <QRegularExpression>
#include <QScreen>
#include <QStyle>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTabletEvent>
#include <QTranslator>
#include <QUrl>
#include <QWidget>
#include <memory>
#if defined(Q_OS_MACOS)
#	include "desktop/utils/macui.h"
#	include "desktop/widgets/macmenu.h"
#	include <QTimer>
#elif defined(Q_OS_WIN)
#	include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#	include "desktop/bundled/kis_tablet/kis_tablet_support_win8.h"
#elif defined(Q_OS_ANDROID)
#	include "libshared/util/androidutils.h"
#elif defined(__EMSCRIPTEN__)
#	include "libclient/wasmsupport.h"
#endif
#ifdef HAVE_PROXY_STYLE
#	include "desktop/utils/fusionui.h"
#endif
#ifdef HAVE_RUN_IN_NEW_PROCESS
#	include <QProcess>
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
		openPath(fe->file(), false);
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
			openStart(QString(), false);
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
	}
#endif

#ifdef HAVE_PROXY_STYLE
	if(!foundStyle && fusionui::looksLikeFusionStyle(themeStyle)) {
		foundStyle = true;
		setStyle(new fusionui::FusionProxyStyle(themeStyle));
	}
#endif

	if(!foundStyle) {
		foundStyle =
			setStyle(themeStyle.isEmpty() ? m_originalSystemStyle : themeStyle);
	}

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
	using libclient::settings::CanvasImplementation;
	int canvasImplementation;
	if(QStringLiteral("system").compare(arg, Qt::CaseInsensitive) == 0) {
		canvasImplementation = int(CanvasImplementation::Default);
	} else if(
		QStringLiteral("qgraphicsview").compare(arg, Qt::CaseInsensitive) ==
		0) {
		canvasImplementation = int(CanvasImplementation::GraphicsView);
	} else if(QStringLiteral("opengl").compare(arg, Qt::CaseInsensitive) == 0) {
		canvasImplementation = int(CanvasImplementation::OpenGl);
	} else if(
		QStringLiteral("software").compare(arg, Qt::CaseInsensitive) == 0) {
		canvasImplementation = int(CanvasImplementation::Software);
	} else {
		if(QStringLiteral("none").compare(arg, Qt::CaseInsensitive) != 0) {
			qWarning("Unknown --renderer '%s'", qUtf8Printable(arg));
		}
		canvasImplementation = m_settings.renderCanvas();
		m_canvasImplementationFromSettings = true;
	}
	m_canvasImplementation = getCanvasImplementationFor(canvasImplementation);
}

void DrawpileApp::initInterface()
{
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

int DrawpileApp::getCanvasImplementationFor(int canvasImplementation)
{
	using libclient::settings::CanvasImplementation;
	switch(canvasImplementation) {
	case int(CanvasImplementation::GraphicsView):
	case int(CanvasImplementation::OpenGl):
	case int(CanvasImplementation::Software):
		return canvasImplementation;
	default:
		return int(CANVAS_IMPLEMENTATION_DEFAULT);
	}
}

void DrawpileApp::initBrushPresets()
{
	Q_ASSERT(!m_brushPresets);
	m_brushPresets = new brushes::BrushPresetTagModel(this);
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

void DrawpileApp::setNewProcessArgs(
	const QCommandLineParser &parser,
	const QVector<const QCommandLineOption *> &options)
{
#ifdef HAVE_RUN_IN_NEW_PROCESS
	m_newProcessArgs.clear();
	for(const QCommandLineOption *option : options) {
		if(parser.isSet(*option)) {
			m_newProcessArgs.append(
				QStringLiteral("--") + option->names().first());
			m_newProcessArgs.append(parser.value(*option));
		}
	}
#else
	Q_UNUSED(parser);
	Q_UNUSED(options);
#endif
}

bool DrawpileApp::runInNewProcess(const QStringList &args)
{
#ifdef HAVE_RUN_IN_NEW_PROCESS
	// Escape hatch for testing and unexpected environments.
	if(isEnvTrue("DRAWPILE_SINGLE_PROCESS")) {
		qDebug("runInNewProcess: DRAWPILE_SINGLE_PROCESS is set");
		return false;
	}

	QString path = QCoreApplication::applicationFilePath();
	if(path.isEmpty()) {
		qWarning("runInNewProcess: no application path");
		return false;
	}

	// These checks are technically superfluous, but they give better logs.
	QFileInfo pathInfo(path);
	if(!pathInfo.isFile()) {
		qWarning(
			"runInNewProcess: application path '%s' doesn't refer to a file",
			qUtf8Printable(path));
		return false;
	}

#	ifdef Q_OS_WIN
	// On Windows, file extensions determine executableness. Qt considers exe,
	// com and bat executables, but we know that Drawpile is only ever supposed
	// to be an exe, so we can narrow down our check to only that extension.
	if(!path.endsWith(".exe", Qt::CaseInsensitive)) {
		qWarning(
			"runInNewProcess: application path '%s' is not an exe file",
			qUtf8Printable(path));
		return false;
	}
#	else
	if(!pathInfo.isExecutable()) {
		qWarning(
			"runInNewProcess: application path '%s' is not executable",
			qUtf8Printable(path));
		return false;
	}
#	endif

	QStringList combinedArgs;
	combinedArgs.reserve(m_newProcessArgs.size() + args.size());
	combinedArgs.append(m_newProcessArgs);
	combinedArgs.append(args);
	qDebug() << "runInNewProcess:" << path << combinedArgs;
	setOverrideCursor(Qt::WaitCursor);
	bool started = QProcess::startDetached(path, combinedArgs);
	if(started) {
		// Give the user some kind of indication that there's another instance
		// of Drawpile booting up. 3 seconds should be long enough to cover it
		// hopefully. Also just kinda hoping that the application will start up
		// okay, rather than concocting some kind of cross-process notification.
		QTimer::singleShot(3000, &DrawpileApp::restoreOverrideCursor);
		return true;
	} else {
		restoreOverrideCursor();
		return false;
	}
#else
	Q_UNUSED(args);
	return false;
#endif
}

bool DrawpileApp::isEnvTrue(const char *key)
{
	QByteArray value = qgetenv(key).trimmed();
	if(!value.isEmpty()) {
		bool ok;
		int i = value.toInt(&ok);
		if(!ok || i != 0) {
			return true;
		}
	}
	return false;
}

MainWindow *
DrawpileApp::acquireWindow(bool restoreWindowPosition, bool singleSession)
{
	if(singleSession) {
		return new MainWindow(restoreWindowPosition, singleSession);
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

void DrawpileApp::openPath(const QString &path, bool restoreWindowPosition)
{
	acquireWindow(restoreWindowPosition, false)->openPath(path);
}

void DrawpileApp::joinUrl(
	const QUrl &url, const QString &autoRecordPath, bool restoreWindowPosition,
	bool singleSession)
{
	acquireWindow(restoreWindowPosition, singleSession)
		->autoJoin(url, autoRecordPath);
}

void DrawpileApp::openBlank(
	int width, int height, QColor backgroundColor, bool restoreWindowPosition)
{
	if(width <= 0) {
		width = m_settings.newCanvasSize().width();
	}
	if(height <= 0) {
		height = m_settings.newCanvasSize().height();
	}
	if(!backgroundColor.isValid()) {
		backgroundColor = m_settings.newCanvasBackColor();
	}
	acquireWindow(restoreWindowPosition, false)
		->newDocument(QSize(width, height), backgroundColor);
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

void DrawpileApp::openStart(const QString &page, bool restoreWindowPosition)
{
	MainWindow *win = new MainWindow(restoreWindowPosition);
	win->newDocument(
		m_settings.newCanvasSize(), m_settings.newCanvasBackColor());
	// Importing an animation is not actually a start dialog page, it's just
	// here as an internal option to let us start a new process if the user
	// requests an animation import on a dirty canvas.
	if(page.compare(
		   QStringLiteral("import-animation-frames"), Qt::CaseInsensitive) ==
	   0) {
		win->importAnimationFrames();
	} else if(
		page.compare(
			QStringLiteral("import-animation-layers"), Qt::CaseInsensitive) ==
		0) {
		win->importAnimationLayers();
	} else if(page.compare(QStringLiteral("none"), Qt::CaseInsensitive) != 0) {
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
#ifdef __EMSCRIPTEN__
	Q_UNUSED(locale);
	langs.append(QStringLiteral("en_US"));
#else
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
#endif
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

struct StartupOptions {
	QStringList files;
	QString startPage;
	bool singleSession = false;
	bool restoreWindowPosition = false;
	QString autoRecordPath;
	QString joinUrl;
	QString openPath;
	bool blank = false;
	int blankWidth = 0;
	int blankHeight = 0;
	QColor blankColor;
};

// Initialize the application and return a list of files to be opened (if any)
static StartupOptions initApp(DrawpileApp &app)
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

	// --buffer
	QCommandLineOption buffer(
		"buffer",
		"Set swap bufferring, 1 for single-, 2 for double-, 3 for "
		"triple-buffering. Any other value uses the system default.",
		"count");
	parser.addOption(buffer);

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

	QCommandLineOption join(
		QStringLiteral("join"),
		QStringLiteral(
			"Open the given session. Analogous to passing it as a positional "
			"argument, but without guessing if it's a file or session URL."),
		QStringLiteral("url"));
	parser.addOption(join);

	QCommandLineOption open(
		QStringLiteral("open"),
		QStringLiteral(
			"Open the given file. Analogous to passing it as a positional "
			"argument, but without guessing if it's a file or a session URL."),
		QStringLiteral("path"));
	parser.addOption(open);

	QCommandLineOption blank(
		QStringLiteral("blank"),
		QStringLiteral("Start Drawpile with a blank canvas of the given width, "
					   "height and background color in (a)rgb format without "
					   "'#', separated by 'x'. Each element is may be left "
					   "blank to use the default instead."),
		QStringLiteral("widthxheightxcolor"));
	parser.addOption(blank);

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

	QCommandLineOption noRestoreWindowPosition(
		QStringLiteral("no-restore-window-position"),
		QStringLiteral("Don't restore main window to its previous position."));
	parser.addOption(noRestoreWindowPosition);

	QCommandLineOption autoRecord(
		QStringLiteral("auto-record"),
		QStringLiteral("Automatically record to the given file. Only used if a "
					   "session URL to join is also given."),
		QStringLiteral("path"));
	parser.addOption(autoRecord);

	QCommandLineOption renderer(
		"renderer",
		"Override canvas renderer, one of: none (don't override, the default), "
		"system (override with system default), qgraphicsview or opengl.",
		"renderer", "none");
	parser.addOption(renderer);

	// URL
	parser.addPositionalArgument("url", "Filename or URL.");

	parser.process(app);
	app.setNewProcessArgs(
		parser, {&dataDir, &portableDataDir, &opengl, &buffer, &renderer,
#ifdef Q_OS_WIN
				 &copyLegacySettings
#endif
				});

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
		// Disable finerpainting if this device is detected to have a stylus.
		bool hasStylus = utils::androidHasStylusInput();
		settings.setOneFingerTouch(
			int(hasStylus ? desktop::settings::OneFingerTouchAction::Draw
						  : desktop::settings::OneFingerTouchAction::Guess));
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

	// Set override locale from settings, use system locale if no override set.
	QLocale locale = QLocale::c();
	QString overrideLang = settings.language();
	if(!overrideLang.isEmpty()) {
		locale = QLocale(overrideLang);
	}
	if(locale == QLocale::c()) {
		locale = QLocale::system();
	}

	initTranslations(app, locale);
	app.initBrushPresets();

	StartupOptions startupOptions;
	startupOptions.files = parser.positionalArguments();
	startupOptions.startPage = parser.value(startPage);
	startupOptions.singleSession = parser.isSet(singleSession);
	startupOptions.restoreWindowPosition =
		!parser.isSet(noRestoreWindowPosition);
	startupOptions.autoRecordPath = parser.value(autoRecord);
	startupOptions.openPath = parser.value(open);
	startupOptions.joinUrl = parser.value(join);
	startupOptions.blank = parser.isSet(blank);
	if(startupOptions.blank) {
		QRegularExpression blankRe(
			QStringLiteral("\\A\\s*([0-9]*)(?:\\s*x\\s*([0-9]*)\\s*"
						   "(?:x\\s*#?([0-9a-f]*))?)?\\s*\\z"),
			QRegularExpression::CaseInsensitiveOption);
		QRegularExpressionMatch match = blankRe.match(parser.value(blank));
		if(match.hasMatch()) {
			startupOptions.blankWidth = match.captured(1).toInt();
			startupOptions.blankHeight = match.captured(2).toInt();
			startupOptions.blankColor =
				QColor(QStringLiteral("#") + match.captured(3));
		}
	}
	return startupOptions;
}

#ifdef __EMSCRIPTEN__
extern "C" int drawpileShouldPreventUnload()
{
	QApplication *app = qApp;
	if(app) {
		static_cast<DrawpileApp *>(app)->settings().trySubmit();
		for(QWidget *widget : app->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw && mw->shouldPreventUnload()) {
				return true;
			}
		}
	}
	return false;
}

extern "C" void drawpileHandleMouseLeave()
{
	QApplication *app = qApp;
	if(app) {
		for(QWidget *widget : app->topLevelWidgets()) {
			MainWindow *mw = qobject_cast<MainWindow *>(widget);
			if(mw) {
				mw->handleMouseLeave();
			}
		}
	}
}
#endif

static int applyRenderSettingsFrom(const QString &path)
{
	QSettings cfg(path, QSettings::IniFormat);

	bool overrideScaling = false;
	if(qgetenv("QT_SCALE_FACTOR").isEmpty()) {
		if(cfg.value(
				  QStringLiteral("scaling_override"), SCALING_OVERRIDE_DEFAULT)
			   .toBool()) {
			qreal factor = qBound(
				1.0,
				cfg.value(QStringLiteral("scaling_factor"), 100).toInt() /
					100.0,
				4.0);
			qputenv("QT_SCALE_FACTOR", qUtf8Printable(QString::number(factor)));
			overrideScaling = true;
		}
	}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	if(qgetenv("QT_AUTO_SCREEN_SCALE_FACTOR").isEmpty()) {
		qputenv("QT_AUTO_SCREEN_SCALE_FACTOR", overrideScaling ? "0" : "1");
	}
#else
	if(qgetenv("QT_ENABLE_HIGHDPI_SCALING").isEmpty()) {
		qputenv("QT_ENABLE_HIGHDPI_SCALING", overrideScaling ? "0" : "1");
	}
#endif

	if(qgetenv("QT_SCALE_FACTOR_ROUNDING_POLICY").isEmpty()) {
		QApplication::setHighDpiScaleFactorRoundingPolicy(
			Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
	}

	bool vsyncOk;
	int vsync = qgetenv("DRAWPILE_VSYNC").toInt(&vsyncOk);
	if(!vsyncOk) {
		vsync = cfg.value(QStringLiteral("vsync")).toInt();
	}
	return vsync;
}

static void applyRenderSettings(
	int argc, char **argv, int &outVsync,
	QSurfaceFormat::SwapBehavior &outSwapBehavior)
{
	QString fileName = QStringLiteral("scaling.ini");
	bool applied = false;
	for(int i = 1; i < argc - 1; ++i) {
		if(!applied && strcmp(argv[i], "--portable-data-dir") == 0) {
			QString path =
				QDir(QString::fromUtf8(argv[++i]) + QStringLiteral("/settings"))
					.absoluteFilePath(fileName);
			outVsync = applyRenderSettingsFrom(path);
			applied = true;
		} else if(strcmp(argv[i], "--opengl") == 0 && i + 1 < argc) {
			qputenv("QT_OPENGL", argv[++i]);
		} else if(strcmp(argv[i], "--buffer") == 0 && i + 1 < argc) {
			switch(QString::fromUtf8(argv[i + 1]).toInt()) {
			case 1:
				outSwapBehavior = QSurfaceFormat::SingleBuffer;
				break;
			case 2:
				outSwapBehavior = QSurfaceFormat::DoubleBuffer;
				break;
			case 3:
				outSwapBehavior = QSurfaceFormat::TripleBuffer;
				break;
			default:
				outSwapBehavior = QSurfaceFormat::DefaultSwapBehavior;
				break;
			}
		}
	}
	if(!applied) {
		// Can't use AppConfigLocation because the application name is not
		// initialized yet and doing so at this point corrupts the main settings
		// file. QSettings is terrible and we should really do away with it.
		outVsync = applyRenderSettingsFrom(
			QStandardPaths::writableLocation(
				QStandardPaths::GenericConfigLocation) +
			QStringLiteral("/drawpile/") + fileName);
	}
}

static void startApplication(DrawpileApp *app)
{
	StartupOptions startupOptions = initApp(*app);
	if(!startupOptions.joinUrl.isEmpty()) {
		app->joinUrl(
			QUrl(startupOptions.joinUrl), startupOptions.autoRecordPath,
			startupOptions.restoreWindowPosition, startupOptions.singleSession);
	} else if(!startupOptions.openPath.isEmpty()) {
		app->openPath(
			startupOptions.openPath, startupOptions.restoreWindowPosition);
	} else if(startupOptions.blank) {
		app->openBlank(
			startupOptions.blankWidth, startupOptions.blankHeight,
			startupOptions.blankColor, startupOptions.restoreWindowPosition);
	} else if(!startupOptions.files.isEmpty()) {
		const QString &arg = startupOptions.files.at(0);
		bool looksLikeSessionUrl =
			arg.startsWith("drawpile://", Qt::CaseInsensitive) ||
			arg.startsWith("ws://", Qt::CaseInsensitive) ||
			arg.startsWith("wss://", Qt::CaseInsensitive);
		if(looksLikeSessionUrl) {
			app->joinUrl(
				QUrl(arg), startupOptions.autoRecordPath,
				startupOptions.restoreWindowPosition,
				startupOptions.singleSession);
		} else {
			app->openPath(arg, startupOptions.restoreWindowPosition);
		}
	} else {
		app->openStart(
			startupOptions.startPage, startupOptions.restoreWindowPosition);
	}
}


int main(int argc, char **argv)
{
#ifdef __EMSCRIPTEN__
	browser::mountPersistentFileSystem(argc, argv);
}

extern "C" void drawpileMain(int argc, char **argv)
{
#endif

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
	// Android uses native dialogs for message boxes and the like, whose
	// behavior is busted in various ways and who don't react like they're
	// supposed to. We use proper Qt dialogs instead.
	qputenv("QT_USE_ANDROID_NATIVE_DIALOGS", "0");
#endif

	// Don't compress input events, that causes jaggy lines on slow devices.
	QApplication::setAttribute(Qt::AA_CompressHighFrequencyEvents, false);
	QApplication::setAttribute(Qt::AA_CompressTabletEvents, false);

#ifdef __EMSCRIPTEN__
	if(browser::hasLowPressurePen()) {
		desktop::settings::globalPressureCurveDefault =
			QStringLiteral("0,0;0.48,0.96;0.5,1;1,1;");
	}
#endif

	int vsync = 0;
	QSurfaceFormat::SwapBehavior swapBehavior =
		QSurfaceFormat::DefaultSwapBehavior;
	applyRenderSettings(argc, argv, vsync, swapBehavior);

#ifdef Q_OS_WIN
	// On Windows, Qt will default to using OpenGL directly if the graphics
	// driver supports it. That's a bad idea though, because graphics drivers on
	// Windows are broken. Default to ANGLE instead. Qt5 ships with ANGLE, but
	// Qt6 doesn't anymore. That can be looked at whenever we switch though.
	if(qgetenv("QT_OPENGL").isEmpty()) {
		qputenv("QT_OPENGL", "angle");
	}
#endif

	// OpenGL rendering format. 8 bit color, no alpha, no depth. Stencil buffer
	// is needed for QPainter, without it, it can't manage to draw large paths
	// and spams the warning log with "Painter path exceeds +/-32767 pixels."
	QSurfaceFormat format = QSurfaceFormat::defaultFormat();
	if(vsync >= 0) {
		format.setSwapInterval(vsync);
	}
	format.setSwapBehavior(swapBehavior);
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
	DP_QT_LOCALE_RESET();

	compat::disableImageReaderAllocationLimit();
	startApplication(app);

#ifndef __EMSCRIPTEN__
	return app->exec();
#endif
}
