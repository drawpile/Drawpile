// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QActionGroup>
#include <QIcon>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QDesktopServices>
#include <QScreen>
#include <QUrl>
#include <QLabel>
#include <QMessageBox>
#include <QInputDialog>
#include <QProgressDialog>
#include <QCloseEvent>
#include <QPushButton>
#include <QToolButton>
#include <QImageReader>
#include <QImageWriter>
#include <QShortcutEvent>
#include <QSplitter>
#include <QMimeData>
#include <QFile>
#include <QWindow>
#include <QVBoxLayout>
#include <QTimer>
#include <QTextEdit>
#include <QThreadPool>
#include <QKeySequence>
#include <QJsonDocument>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTemporaryFile>

#ifdef Q_OS_MACOS
static constexpr auto CTRL_KEY = Qt::META;
#include "desktop/widgets/macmenu.h"
#else
static constexpr auto CTRL_KEY = Qt::CTRL;
#endif

#include "desktop/mainwindow.h"
#include "libclient/document.h"
#include "desktop/main.h"
#include "desktop/tabletinput.h"

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/selectionmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "desktop/view/canvaswrapper.h"
#include "desktop/view/lock.h"
#include "desktop/widgets/canvasframe.h"
#include "desktop/scene/selectionitem.h"
#include "desktop/scene/toggleitem.h"
#include "libclient/canvas/userlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/documentmetadata.h"

#include "libshared/util/whatismyip.h"
#include "cmake-config/config.h"
#include "libclient/utils/images.h"
#include "libshared/util/networkaccess.h"
#include "libshared/util/paths.h"
#include "libshared/util/qtcompat.h"
#include "libclient/utils/shortcutdetector.h"
#include "libclient/utils/customshortcutmodel.h"
#include "libclient/utils/logging.h"
#include "desktop/utils/actionbuilder.h"
#include "desktop/utils/widgetutils.h"

#include "desktop/widgets/dualcolorbutton.h"
#include "desktop/widgets/viewstatus.h"
#include "desktop/widgets/viewstatusbar.h"
#include "desktop/widgets/netstatus.h"
#include "desktop/chat/chatbox.h"

#include "desktop/docks/toolsettingsdock.h"
#include "desktop/docks/brushpalettedock.h"
#include "desktop/docks/navigator.h"
#include "desktop/docks/colorpalette.h"
#include "desktop/docks/colorspinner.h"
#include "desktop/docks/colorsliders.h"
#include "desktop/docks/layerlistdock.h"
#include "desktop/docks/onionskins.h"
#include "desktop/docks/timeline.h"
#include "desktop/docks/titlewidget.h"

#include "libclient/net/client.h"
#include "libclient/net/login.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/import/canvasloaderrunnable.h"
#include "libclient/parentalcontrols/parentalcontrols.h"

#include "libclient/tools/toolcontroller.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/toolwidgets/transformsettings.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/inspectorsettings.h"

#include "desktop/filewrangler.h"
#include "libclient/export/animationsaverrunnable.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/drawdance/perf.h"

#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/invitedialog.h"
#include "desktop/dialogs/layoutsdialog.h"
#include "desktop/dialogs/logindialog.h"
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/resizedialog.h"
#include "desktop/dialogs/playbackdialog.h"
#include "desktop/dialogs/dumpplaybackdialog.h"
#include "desktop/dialogs/resetdialog.h"
#include "desktop/dialogs/sessionsettings.h"
#include "desktop/dialogs/serverlogdialog.h"
#include "desktop/dialogs/tablettester.h"
#include "desktop/dialogs/touchtestdialog.h"
#include "desktop/dialogs/abusereport.h"
#include "desktop/dialogs/brushsettingsdialog.h"
#include "desktop/dialogs/sessionundodepthlimitdialog.h"
#include "desktop/dialogs/userinfodialog.h"
#include "desktop/dialogs/startdialog.h"
#include "desktop/dialogs/animationimportdialog.h"
#include "desktop/dialogs/systeminfodialog.h"
#include "libclient/import/loadresult.h"
#include <functional>

#ifdef Q_OS_WIN
#include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#endif

#ifdef __EMSCRIPTEN__
#	include "libclient/wasmsupport.h"
#else
#	include "desktop/utils/recents.h"
#endif

#ifdef DP_HAVE_BUILTIN_SERVER
#	include "libclient/server/builtinserver.h"
#endif

using std::placeholders::_1;
using std::placeholders::_2;
using desktop::settings::Settings;
// Totally arbitrary nonsense
constexpr auto DEBOUNCE_MS = 250;

MainWindow::MainWindow(bool restoreWindowPosition, bool singleSession)
	: QMainWindow(),
	  m_singleSession(singleSession),
	  m_smallScreenMode(isInitialSmallScreenMode()),
	  m_updatingInterfaceMode(true),
	  m_splitter(nullptr),
	  m_dockToolSettings(nullptr),
	  m_dockBrushPalette(nullptr),
	  m_dockInput(nullptr),
	  m_dockLayers(nullptr),
	  m_dockColorPalette(nullptr),
	  m_dockNavigator(nullptr),
	  m_dockOnionSkins(nullptr),
	  m_dockTimeline(nullptr),
	  m_chatbox(nullptr),
	  m_viewLock(nullptr),
	  m_canvasView(nullptr),
	  m_viewStatusBar(nullptr),
	  m_lockstatus(nullptr),
	  m_netstatus(nullptr),
	  m_viewstatus(nullptr),
	  m_statusChatButton(nullptr),
	  m_playbackDialog(nullptr),
	  m_dumpPlaybackDialog(nullptr),
	  m_sessionSettings(nullptr),
	  m_serverLogDialog(nullptr),
#ifndef __EMSCRIPTEN__
	  m_recentMenu(nullptr),
#endif
	  m_lastLayerViewMode(nullptr),
	  m_currentdoctools(nullptr),
	  m_admintools(nullptr),
	  m_canvasbgtools(nullptr),
	  m_resizetools(nullptr),
	  m_putimagetools(nullptr),
	  m_undotools(nullptr),
	  m_drawingtools(nullptr),
	  m_brushSlots(nullptr),
	  m_dockToggles(nullptr),
#ifndef SINGLE_MAIN_WINDOW
	  m_fullscreenOldMaximized(false),
#endif
	  m_tempToolSwitchShortcut(nullptr),
	  m_titleBarsHidden(false),
	  m_wasSessionLocked(false),
	  m_notificationsMuted(false),
	  m_initialCatchup(false),
	  m_doc(nullptr)
#ifndef __EMSCRIPTEN__
	  , m_exitAction(RUNNING)
#endif
{
	// Avoid flickering of intermediate states.
	setUpdatesEnabled(false);

	m_saveSplitterDebounce.setSingleShot(true);
	m_saveWindowDebounce.setSingleShot(true);
	m_saveSplitterDebounce.setInterval(DEBOUNCE_MS);
	m_saveWindowDebounce.setInterval(DEBOUNCE_MS);

	// The document (initially empty)
	desktop::settings::Settings &settings = dpApp().settings();
	m_doc = new Document(dpApp().canvasImplementation(), settings, this);

	// Set up the main window widgets
	// The central widget consists of a custom status bar and a splitter
	// which includes the chat box and the main view.
	// We don't use the normal QMainWindow statusbar to save some vertical space for the docks.
	QWidget *centralwidget = new QWidget;
	QVBoxLayout *mainwinlayout = new QVBoxLayout(centralwidget);
	mainwinlayout->setContentsMargins(0, 0, 0 ,0);
	mainwinlayout->setSpacing(0);
	setCentralWidget(centralwidget);

	// Work area is split between the canvas view and the chatbox
	m_splitter = new QSplitter(Qt::Vertical, centralwidget);
	m_splitterOriginalHandleWidth = m_splitter->handleWidth();

	mainwinlayout->addWidget(m_splitter);

	// Create custom status bar
	m_viewStatusBar = new widgets::ViewStatusBar;
	m_viewStatusBar->setSizeGripEnabled(false);
	mainwinlayout->addWidget(m_viewStatusBar);

	// Create status indicator widgets
	m_viewstatus = new widgets::ViewStatus(this);
	m_viewstatus->setHidden(m_smallScreenMode);

	m_netstatus = new widgets::NetStatus(this);
	m_lockstatus = new QLabel(this);
	m_lockstatus->setFixedSize(QSize(16, 16));

	m_viewStatusBar->addPermanentWidget(m_viewstatus);
	m_viewStatusBar->addPermanentWidget(m_netstatus);

	// Statusbar chat button: this is normally hidden and only shown
	// when there are unread chat messages.
	m_statusChatButton = new QToolButton(this);
	m_statusChatButton->setAutoRaise(true);
	m_statusChatButton->setIcon(QIcon::fromTheme("drawpile_chat"));
	utils::setWidgetRetainSizeWhenHidden(m_statusChatButton, true);
	m_statusChatButton->hide();
	m_viewStatusBar->addPermanentWidget(m_statusChatButton);

	m_viewStatusBar->addPermanentWidget(m_lockstatus);

	m_viewLock = new view::Lock(this);

	int SPLITTER_WIDGET_IDX = 0;

	// Create canvas view (first splitter item)
	m_canvasView =
		view::CanvasWrapper::instantiate(dpApp().canvasImplementation(), this);
	m_canvasView->setShowToggleItems(m_smallScreenMode);

	m_canvasFrame = new widgets::CanvasFrame(m_canvasView->viewWidget());
	m_splitter->addWidget(m_canvasFrame);
	m_splitter->setCollapsible(SPLITTER_WIDGET_IDX++, false);

	// Create the chatbox
	m_chatbox = new widgets::ChatBox(m_doc, m_smallScreenMode, this);
	m_splitter->addWidget(m_chatbox);

	connect(m_chatbox, &widgets::ChatBox::reattachNowPlease, this, [this]() {
		m_splitter->addWidget(m_chatbox);
	});

	// Nice initial division between canvas and chat
	{
		const int h = height();
		m_splitter->setSizes(QList<int>() << (h * 2 / 3) << (h / 3));
	}

	m_dualColorButton = new widgets::DualColorButton(this);

	// Create docks
	createDocks();
	resetDefaultDocks();

	// Crete persistent dialogs
	m_sessionSettings = new dialogs::SessionSettingsDialog(m_doc, this);
	m_serverLogDialog = new dialogs::ServerLogDialog(this);
	m_serverLogDialog->setModel(m_doc->serverLog());

	// Document <-> Main window connections
	connect(m_doc, &Document::canvasChanged, this, &MainWindow::onCanvasChanged);
	connect(m_doc, &Document::canvasSaveStarted, this, &MainWindow::onCanvasSaveStarted);
	connect(m_doc, &Document::canvasSaved, this, &MainWindow::onCanvasSaved);
#ifdef __EMSCRIPTEN__
	connect(m_doc, &Document::canvasDownloadStarted, this, &MainWindow::onCanvasDownloadStarted);
	connect(m_doc, &Document::canvasDownloadReady, this, &MainWindow::onCanvasDownloadReady);
	connect(m_doc, &Document::canvasDownloadError, this, &MainWindow::onCanvasDownloadError);
#endif
	connect(m_doc, &Document::templateExported, this, &MainWindow::onTemplateExported);
	connect(m_doc, &Document::dirtyCanvas, this, &MainWindow::setWindowModified);
	connect(m_doc, &Document::sessionTitleChanged, this, &MainWindow::updateTitle);
	connect(m_doc, &Document::currentPathChanged, this, &MainWindow::updateTitle);
	connect(m_doc, &Document::recorderStateChanged, this, &MainWindow::setRecorderStatus);
	connect(m_doc, &Document::sessionResetState, this, &MainWindow::showResetNoticeDialog, Qt::QueuedConnection);

	connect(m_doc, &Document::autoResetTooLarge, this, [this](int maxSize) {
		m_doc->sendLockSession(true);
		auto *msgbox = new QMessageBox(
					QMessageBox::Warning,
					tr("Server out of space"),
					tr("Server is running out of history space and session has grown too large to automatically reset! (Limit is %1 MB)\nSimplify the canvas and reset manually before space runs out.")
						.arg(maxSize / double(1024*1024), 0, 'f', 2),
					QMessageBox::Ok,
					this
					);
		msgbox->show();
	});

	// Tool dock connections
	m_tempToolSwitchShortcut = new ShortcutDetector(this);

	m_canvasView->connectCanvasFrame(m_canvasFrame);
	m_canvasView->connectDocument(m_doc);
	m_canvasView->connectMainWindow(this);
	m_canvasView->connectNavigator(m_dockNavigator);
	m_canvasView->connectLock(m_viewLock);
	m_canvasView->connectViewStatus(m_viewstatus);
	m_canvasView->connectViewStatusBar(m_viewStatusBar);
	m_canvasView->connectToolSettings(m_dockToolSettings);

	connect(m_dockToolSettings->brushSettings(), &tools::BrushSettings::brushSettingsDialogRequested,
		this, &MainWindow::showBrushSettingsDialog);

	connect(m_dockLayers, &docks::LayerList::layerSelected, this, &MainWindow::updateLockWidget);
	connect(m_dockLayers, &docks::LayerList::activeLayerVisibilityChanged, this, &MainWindow::updateLockWidget);

	connect(m_dockToolSettings, &docks::ToolSettings::toolChanged, this, &MainWindow::toolChanged);
	connect(m_dockToolSettings, &docks::ToolSettings::activeBrushChanged, this, &MainWindow::updateLockWidget);

	// Color docks
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorPalette, &docks::ColorPaletteDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorSpinner, &docks::ColorSpinnerDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorSliders, &docks::ColorSliderDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::lastUsedColorsChanged, m_dockColorPalette, &docks::ColorPaletteDock::setLastUsedColors);
	connect(m_dockToolSettings, &docks::ToolSettings::lastUsedColorsChanged, m_dockColorSpinner, &docks::ColorSpinnerDock::setLastUsedColors);
	connect(m_dockToolSettings, &docks::ToolSettings::lastUsedColorsChanged, m_dockColorSliders, &docks::ColorSliderDock::setLastUsedColors);
	connect(m_dockColorPalette, &docks::ColorPaletteDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(m_dockColorSpinner, &docks::ColorSpinnerDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(m_dockColorSliders, &docks::ColorSliderDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);

	// Dual color button
	connect(
		m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged,
		m_dualColorButton, &widgets::DualColorButton::setForegroundColor);
	connect(
		m_dockToolSettings, &docks::ToolSettings::backgroundColorChanged,
		m_dualColorButton, &widgets::DualColorButton::setBackgroundColor);
	connect(
		m_dualColorButton, &widgets::DualColorButton::foregroundClicked,
		m_dockToolSettings, &docks::ToolSettings::changeForegroundColor);
	connect(
		m_dualColorButton, &widgets::DualColorButton::backgroundClicked,
		m_dockToolSettings, &docks::ToolSettings::changeBackgroundColor);
	connect(
		m_dualColorButton, &widgets::DualColorButton::swapClicked,
		m_dockToolSettings, &docks::ToolSettings::swapColors);
	connect(
		m_dualColorButton, &widgets::DualColorButton::resetClicked,
		m_dockToolSettings, &docks::ToolSettings::resetColors);

	// Network client <-> UI connections
	connect(m_doc, &Document::catchupProgress, this, &MainWindow::updateCatchupProgress);
	connect(m_doc, &Document::catchupProgress, m_netstatus, &widgets::NetStatus::setCatchupProgress);

	connect(
		m_doc->client(), &net::Client::serverStatusUpdate, m_viewStatusBar,
		&widgets::ViewStatusBar::setSessionHistorySize);
	connect(
		m_doc->client(), &net::Client::lagMeasured, m_viewStatusBar,
		&widgets::ViewStatusBar::setLatency);

	connect(m_chatbox, &widgets::ChatBox::message, m_doc->client(), &net::Client::sendMessage);
	connect(m_dockTimeline, &docks::Timeline::timelineEditCommands, m_doc->client(), &net::Client::sendMessages);

	connect(m_serverLogDialog, &dialogs::ServerLogDialog::opCommand, m_doc->client(), &net::Client::sendMessage);
	connect(m_dockLayers, &docks::LayerList::layerCommands, m_doc->client(), &net::Client::sendMessages);

	connect(m_doc->client(), &net::Client::userInfoRequested, this, &MainWindow::sendUserInfo);
	connect(m_doc->client(), &net::Client::currentBrushRequested, this, &MainWindow::sendCurrentBrush);
	connect(m_doc->client(), &net::Client::currentBrushReceived, this, &MainWindow::receiveCurrentBrush);

	connect(m_doc->client(), &net::Client::bansImported, m_sessionSettings, &dialogs::SessionSettingsDialog::bansImported);
	connect(m_doc->client(), &net::Client::bansExported, m_sessionSettings, &dialogs::SessionSettingsDialog::bansExported);
	connect(m_doc->client(), &net::Client::bansImpExError, m_sessionSettings, &dialogs::SessionSettingsDialog::bansImpExError);
	connect(m_sessionSettings, &dialogs::SessionSettingsDialog::requestBanImport, m_doc->client(), &net::Client::requestBanImport);
	connect(m_sessionSettings, &dialogs::SessionSettingsDialog::requestBanExport, m_doc->client(), &net::Client::requestBanExport);
	connect(m_sessionSettings, &dialogs::SessionSettingsDialog::requestUpdateAuthList, m_doc->client(), &net::Client::requestUpdateAuthList);

	// Tool controller <-> UI connections
	connect(
		m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged,
		this, &MainWindow::updateSelectTransformActions);
	connect(m_doc->toolCtrl(), &tools::ToolController::colorUsed, m_dockToolSettings, &docks::ToolSettings::addLastUsedColor);
	connect(m_doc->toolCtrl(), &tools::ToolController::actionCancelled, m_dockToolSettings->colorPickerSettings(), &tools::ColorPickerSettings::cancelPickFromScreen);
	connect(
		m_doc->toolCtrl(), &tools::ToolController::busyStateChanged, this,
		&MainWindow::setBusy);

	connect(m_dockLayers, &docks::LayerList::layerSelected, m_doc->toolCtrl(), &tools::ToolController::setActiveLayer);
	connect(m_dockLayers, &docks::LayerList::layerSelected, m_dockTimeline, &docks::Timeline::setCurrentLayer);
	connect(m_dockLayers, &docks::LayerList::fillSourceSet,
		m_dockToolSettings->fillSettings(), &tools::FillSettings::setSourceLayerId);
	connect(m_dockTimeline, &docks::Timeline::layerSelected, m_dockLayers, &docks::LayerList::selectLayer);
	connect(m_dockTimeline, &docks::Timeline::trackSelected, m_dockLayers, &docks::LayerList::setTrackId);
	m_dockLayers->setTrackId(m_dockTimeline->currentTrackId());
	connect(m_dockTimeline, &docks::Timeline::frameSelected, m_dockLayers, &docks::LayerList::setFrame);
	m_dockLayers->setFrame(m_dockTimeline->currentFrame());
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged,
			m_dockToolSettings->annotationSettings(), &tools::AnnotationSettings::setSelectionId);
	connect(
		m_dockLayers, &docks::LayerList::layerSelected, m_chatbox,
		&widgets::ChatBox::setCurrentLayer);

	// Network status changes
	connect(m_doc, &Document::serverConnected, this, &MainWindow::onServerConnected);
	connect(m_doc, &Document::serverLoggedIn, this, &MainWindow::onServerLogin);
	connect(m_doc, &Document::serverDisconnected, this, &MainWindow::onServerDisconnected);
	connect(m_doc, &Document::serverDisconnected, this, [this]() {
		m_viewStatusBar->setSessionHistorySize(-1);
		m_viewStatusBar->setLatency(-1);
	});
	connect(m_doc, &Document::compatibilityModeChanged, this, &MainWindow::onCompatibilityModeChanged);
	connect(m_doc, &Document::sessionNsfmChanged, this, &MainWindow::onNsfmChanged);

	connect(m_doc, &Document::serverConnected, m_netstatus, &widgets::NetStatus::connectingToHost);
	connect(m_doc->client(), &net::Client::serverDisconnecting, m_netstatus, &widgets::NetStatus::hostDisconnecting);
	connect(m_doc, &Document::serverDisconnected, m_netstatus, &widgets::NetStatus::hostDisconnected);
	connect(m_sessionSettings, &dialogs::SessionSettingsDialog::joinPasswordChanged, m_netstatus, &widgets::NetStatus::setJoinPassword);
	connect(m_doc, &Document::sessionPasswordChanged, m_netstatus, &widgets::NetStatus::setHaveJoinPassword);
	connect(
		m_doc, &Document::sessionOutOfSpaceChanged, this,
		&MainWindow::updateLockWidget);

	connect(m_doc->client(), SIGNAL(bytesReceived(int)), m_netstatus, SLOT(bytesReceived(int)));
	connect(m_doc->client(), &net::Client::bytesSent, m_netstatus, &widgets::NetStatus::bytesSent);
	connect(m_doc->client(), &net::Client::lagMeasured, m_netstatus, &widgets::NetStatus::lagMeasured);

	connect(&dpApp(), &DrawpileApp::setDockTitleBarsHidden, this, &MainWindow::setDockTitleBarsHidden);
	connect(&dpApp(), &DrawpileApp::focusCanvas, this, [this] {
		if(dpApp().settings().doubleTapAltToFocusCanvas()) {
			m_canvasView->viewWidget()->setFocus();
		}
	});

	// Create actions and menus
	setupActions();
	setDrawingToolsEnabled(false);
	m_dockToolSettings->triggerUpdate();

	// Restore settings and show the window
	updateTitle();
	readSettings(restoreWindowPosition);

	// Set status indicators
	updateLockWidget();
	setRecorderStatus(false);

	// Actually paint the window
	reenableUpdates();

#ifdef Q_OS_MACOS
	MacMenu::instance()->addWindow(this);
#endif

	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

#ifdef SINGLE_MAIN_WINDOW
	dpApp().deleteAllMainWindowsExcept(this);
#endif

	settings.bindInterfaceMode(this, [this](bool) {
		updateInterfaceMode();
	});
	settings.trySubmit();

	m_updatingInterfaceMode = false;
	updateInterfaceMode();
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MACOS
	MacMenu::instance()->removeWindow(this);
#endif

	// Clear this out first so there will be no weird signals emitted
	// while the document is being torn down.
	m_canvasView->disposeScene();

	// Make sure all child dialogs are closed
	for(auto *child : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
		delete child;
	}

	dpApp().settings().trySubmit();
}

void MainWindow::autoJoin(const QUrl &url, const QString &autoRecordPath)
{
	if(m_singleSession) {
		m_doc->client()->setSessionUrl(url);
		joinSession(url, autoRecordPath);
	} else {
		dialogs::StartDialog *dlg = showStartDialog();
		dlg->autoJoin(url, autoRecordPath);
	}
}

void MainWindow::onCanvasChanged(canvas::CanvasModel *canvas)
{
	m_canvasView->setCanvas(canvas);

	connect(canvas->aclState(), &canvas::AclState::localOpChanged, this, &MainWindow::onOperatorModeChange);
	connect(canvas->aclState(), &canvas::AclState::localLockChanged, this, &MainWindow::updateLockWidget);
	connect(canvas->aclState(), &canvas::AclState::resetLockChanged, this, &MainWindow::updateLockWidget);
	connect(canvas->aclState(), &canvas::AclState::featureAccessChanged, this, &MainWindow::onFeatureAccessChange);
	connect(canvas->paintEngine(), &canvas::PaintEngine::undoDepthLimitSet, this, &MainWindow::onUndoDepthLimitSet);

	connect(canvas, &canvas::CanvasModel::chatMessageReceived, this, [this]() {
		// Show a "new message" indicator when the chatbox is collapsed
		const auto sizes = m_splitter->sizes();
		if(sizes.length() > 1 && sizes.at(1)==0)
			m_statusChatButton->show();
	});

	connect(canvas, &canvas::CanvasModel::layerAutoselectRequest, m_dockLayers, &docks::LayerList::selectLayer);
	connect(canvas, &canvas::CanvasModel::colorPicked, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(canvas, &canvas::CanvasModel::colorPicked, m_dockToolSettings->colorPickerSettings(), &tools::ColorPickerSettings::addColor);
	connect(canvas, &canvas::CanvasModel::canvasInspected, m_dockToolSettings->inspectorSettings(), &tools::InspectorSettings::onCanvasInspected);
	connect(canvas, &canvas::CanvasModel::previewAnnotationRequested, m_doc->toolCtrl(), &tools::ToolController::setActiveAnnotation);

	connect(
		canvas->selection(), &canvas::SelectionModel::selectionChanged, this,
		&MainWindow::updateSelectTransformActions);
	connect(
		canvas->transform(), &canvas::TransformModel::transformChanged, this,
		&MainWindow::updateSelectTransformActions);

	connect(canvas, &canvas::CanvasModel::userJoined, this, [this](int, const QString &name) {
		m_viewStatusBar->showMessage(tr("🙋 %1 joined!").arg(name), 2000);
	});

	connect(m_serverLogDialog, &dialogs::ServerLogDialog::inspectModeChanged, canvas, [this, canvas](unsigned int contextId) {
		canvas->inspectCanvas(contextId, m_dockToolSettings->inspectorSettings()->isShowTiles());
	});
	connect(m_serverLogDialog, &dialogs::ServerLogDialog::inspectModeStopped, canvas, &canvas::CanvasModel::stopInspectingCanvas);

	updateLayerViewMode();

	m_dockLayers->setCanvas(canvas);
	m_serverLogDialog->setUserList(canvas->userlist());
	m_dockNavigator->setCanvasModel(canvas);
	m_dockTimeline->setCanvas(canvas);

	connect(m_dockTimeline, &docks::Timeline::frameSelected, canvas->paintEngine(), &canvas::PaintEngine::setViewFrame);
	connect(m_dockTimeline, &docks::Timeline::trackHidden, canvas->paintEngine(), &canvas::PaintEngine::setTrackVisibility);
	connect(m_dockTimeline, &docks::Timeline::trackOnionSkinEnabled, canvas->paintEngine(), &canvas::PaintEngine::setTrackOnionSkin);

	connect(m_dockOnionSkins, &docks::OnionSkinsDock::onionSkinsChanged, canvas->paintEngine(), &canvas::PaintEngine::setOnionSkins);
	m_dockOnionSkins->triggerUpdate();

	m_dockToolSettings->fillSettings()->setLayerList(canvas->layerlist());
	m_dockToolSettings->inspectorSettings()->setUserList(canvas->userlist());

	// Make sure the UI matches the default feature access level
	m_currentdoctools->setEnabled(true);
	setDrawingToolsEnabled(true);
	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		DP_Feature f = DP_Feature(i);
		onFeatureAccessChange(f, m_doc->canvas()->aclState()->canUseFeature(f));
	}
	onUndoDepthLimitSet(canvas->paintEngine()->undoDepthLimit());
	getAction("resetsession")->setEnabled(true);

	updateSelectTransformActions();
}

/**
 * This function is used to check if the current board can be replaced
 * or if a new window is needed to open other content.
 *
 * The window cannot be replaced if any of the following conditions are true:
 * - there are unsaved changes
 * - there is a network connection
 * - session recording is in progress
 * - recording playback is in progress
 * - debug dump playback is in progress
 *
 * @retval false if a new window needs to be created
 */
bool MainWindow::canReplace() const {
	return !(m_doc->isDirty() || m_doc->client()->isConnected() || m_doc->isRecording() || m_playbackDialog || m_dumpPlaybackDialog);
}

#ifdef __EMSCRIPTEN__
bool MainWindow::shouldPreventUnload() const
{
	return m_singleSession ? m_doc->client()->isLoggedIn() : !canReplace();
}

void MainWindow::handleMouseLeave()
{
	m_canvasView->clearKeys();
	dpApp().settings().trySubmit();
}
#endif

void MainWindow::prepareWindowReplacement()
{
	if(windowState().testFlag(Qt::WindowFullScreen)) {
		toggleFullscreen();
	}
	saveWindowState();
	saveSplitterState();
	dpApp().settings().trySubmit();
}

/**
 * The file is added to the list of recent files and the menus on all open
 * mainwindows are updated.
 * @param file filename to add
 */
void MainWindow::addRecentFile(const QString& file)
{
#ifdef __EMSCRIPTEN__
	Q_UNUSED(file);
#else
	utils::Recents &recents = dpApp().recents();
	recents.addFile(file);
#endif
}

/**
 * Set window title according to currently open file and session
 */
void MainWindow::updateTitle()
{
	QString name;
	if(m_doc->currentPath().isEmpty()) {
		name = tr("Untitled");

	} else {
		const QFileInfo info(m_doc->currentPath());
		name = info.completeBaseName();
	}

	if(m_doc->sessionTitle().isEmpty())
		setWindowTitle(QStringLiteral("%1[*]").arg(name));
	else
		setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, m_doc->sessionTitle()));

#ifdef Q_OS_MACOS
	MacMenu::instance()->updateWindow(this);
#endif
}

void MainWindow::setDrawingToolsEnabled(bool enable)
{
	bool actuallyEnabled = enable && m_doc->canvas();
	m_drawingtools->setEnabled(actuallyEnabled);
	m_freehandButton->setEnabled(actuallyEnabled);
}

/**
 * Load customized shortcuts
 */
void MainWindow::loadShortcuts(const QVariantMap &cfg)
{
	static const QRegularExpression shortcutAmpersand { "&([^&])" };

	disconnect(m_textCopyConnection);
	const QKeySequence standardCopyShortcut { QKeySequence::Copy };

	for(auto *a : findChildren<QAction*>()) {
		const QString &name = a->objectName();
		if(!name.isEmpty()) {
			if(cfg.contains(name)) {
				const auto v = cfg.value(name);
				QList<QKeySequence> shortcuts;

				if(v.canConvert<QKeySequence>()) {
					QKeySequence shortcut = v.value<QKeySequence>();
					if(!shortcuts.contains(shortcut)) {
						shortcuts.append(shortcut);
					}
				} else {
					const auto list = v.toList();
					for(const auto &vv : list) {
						if(vv.canConvert<QKeySequence>()) {
							QKeySequence shortcut = vv.value<QKeySequence>();
							if(!shortcuts.contains(shortcut)) {
								shortcuts.append(shortcut);
							}
						}
					}
				}
				a->setShortcuts(shortcuts);

			} else {
				a->setShortcuts(CustomShortcutModel::getDefaultShortcuts(name));
			}

			if(a->shortcut() == standardCopyShortcut) {
				m_textCopyConnection = connect(a, &QAction::triggered, this, &MainWindow::copyText);
			}

			// If an action has a shortcut, show it in the tooltip
			if(a->shortcut().isEmpty()) {
				a->setToolTip(QString());

			} else {
				QString text = a->text();
				text.replace(shortcutAmpersand, QStringLiteral("\\1"));

				// In languages with non-latin alphabets, it's a common
				// convention to add a keyboard shortcut like this:
				// English: &File
				// Japanese: ファイル(&F)
				const int i = text.lastIndexOf('(');
				if(i>0)
					text.truncate(i);

				a->setToolTip(QStringLiteral("%1 (%2)").arg(text, a->shortcut().toString()));
			}

			a->setAutoRepeat(a->property("shortcutAutoRepeats").toBool());
		}
	}

	// Update enabled status of certain actions
	QAction *uncensorAction = getAction("layerviewuncensor");
	const bool canUncensor = !parentalcontrols::isLayerUncensoringBlocked();
	uncensorAction->setEnabled(canUncensor);
	if(!canUncensor) {
		uncensorAction->setChecked(false);
		updateLayerViewMode();
	}

	for(dialogs::StartDialog *dlg : findChildren<dialogs::StartDialog *>(QString{}, Qt::FindDirectChildrenOnly)) {
		setStartDialogActions(dlg);
	}

	for(dialogs::Flipbook *fp : findChildren<dialogs::Flipbook *>(QString{}, Qt::FindDirectChildrenOnly)) {
		fp->setRefreshShortcuts(getAction("showflipbook")->shortcuts());
	}

	updateFreehandToolButton(
		int(m_dockToolSettings->brushSettings()->getBrushMode()));
}

void MainWindow::toggleLayerViewMode()
{
	if(!m_doc->canvas())
		return;
	// If any of the special view modes is triggered again, we want to toggle
	// back to the normal view mode. This allows the user to e.g. switch between
	// single layer and normal mode by mashing the same shortcut. Otherwise
	// pressing the shortcut again would have no useful effect anyway.
	QAction *actions[] = {
		getAction("layerviewcurrentlayer"),
		getAction("layerviewcurrentframe"),
	};
	for (QAction *action : actions) {
		if (action->isChecked() && m_lastLayerViewMode == action) {
			getAction("layerviewnormal")->setChecked(true);
			break;
		}
	}
	updateLayerViewMode();
}

void MainWindow::updateLayerViewMode()
{
	if(!m_doc->canvas())
		return;

	const bool censor = !getAction("layerviewuncensor")->isChecked();

	DP_ViewMode mode;
	QAction *action;
	if((action = getAction("layerviewcurrentlayer"))->isChecked()) {
		mode = DP_VIEW_MODE_LAYER;
	} else if((action = getAction("layerviewcurrentframe"))->isChecked()) {
		mode = DP_VIEW_MODE_FRAME;
	} else {
		action = getAction("layerviewnormal");
		mode = DP_VIEW_MODE_NORMAL;
	}
	m_lastLayerViewMode = action;

	m_doc->canvas()->paintEngine()->setViewMode(mode, censor);
	updateLockWidget();
}

/**
 * Read and apply mainwindow related settings.
 */
void MainWindow::readSettings(bool windowpos)
{
	auto &settings = dpApp().settings();

	settings.bindTabletEraserAction(this, [=](int action) {
#ifdef __EMSCRIPTEN__
		m_canvasView->setEnableEraserOverride(
			action == int(tabletinput::EraserAction::Override));
#else
		if(action == int(tabletinput::EraserAction::Switch)) {
			connect(
				&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings,
				&docks::ToolSettings::switchToEraserSlot, Qt::UniqueConnection);
		} else {
			disconnect(
				&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings,
				&docks::ToolSettings::switchToEraserSlot);
		}
		if(action == int(tabletinput::EraserAction::Override)) {
			connect(
				&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings,
				&docks::ToolSettings::switchToEraserMode, Qt::UniqueConnection);
		} else {
			disconnect(
				&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings,
				&docks::ToolSettings::switchToEraserMode);
		}
#endif
	});

	settings.bindShareBrushSlotColor(m_dockToolSettings->brushSettings(), &tools::BrushSettings::setShareBrushSlotColor);

	// Restore previously used window size and position
	resize(settings.lastWindowSize());
	if(windowpos) {
		utils::moveIfOnScreen(this, settings.lastWindowPosition());
	}

	// Show self
	utils::showWindow(this, settings.lastWindowMaximized(), true);

	// The following state restoration requires the window to be resized, but Qt
	// does that lazily on the next event loop iteration. So we forcefully flush
	// the event loop here to actually get the window resized before continuing.
	dpApp().processEvents();

	if(m_smallScreenMode) {
		initSmallScreenState();
	} else {
		restoreSettings(settings);
	}

	connect(m_splitter, &QSplitter::splitterMoved, this, [=] {
		m_saveSplitterDebounce.start();
	});

	// Restore remembered actions
	m_actionsConfig = settings.lastWindowActions();
	for(auto *act : actions()) {
		if(act->isCheckable() && act->property("remembered").toBool()) {
			act->setChecked(m_actionsConfig.value(act->objectName(), act->property("defaultValue").toBool()));
			connect(act, &QAction::toggled, this, [=, &settings](bool checked) {
				m_actionsConfig[act->objectName()] = checked;
				settings.setLastWindowActions(m_actionsConfig);
			});
		}
	}

	if(m_smallScreenMode) {
		setFreezeDocks(true);
		setDockOptions(dockOptions() | QMainWindow::VerticalTabs);
	}

	// Customize shortcuts
	settings.bindShortcuts(this, &MainWindow::loadShortcuts);

#ifndef __EMSCRIPTEN__
	// Restore recent files
	if(!m_singleSession) {
		dpApp().recents().bindFileMenu(m_recentMenu);
	}
#endif

	connect(&m_saveWindowDebounce, &QTimer::timeout, this, &MainWindow::saveWindowState);
	connect(&m_saveSplitterDebounce, &QTimer::timeout, this, &MainWindow::saveSplitterState);

	// QMainWindow produces no event when there is a change that would cause the
	// serialised state to be different, so we will just have to make some
	// guesses listening for relevant changes in the docked widgets.
	for(auto *w : findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if (w->inherits("QDockWidget") || w->inherits("QToolBar")) {
			w->installEventFilter(this);
		}
	}

#ifdef SINGLE_MAIN_WINDOW
	dpApp().processEvents();
	resize(compat::widgetScreen(*this)->availableSize());
#endif
}

void MainWindow::restoreSettings(const desktop::settings::Settings &settings)
{
	// Restore dock, toolbar and view states
	if(const auto lastWindowState = settings.lastWindowState(); !lastWindowState.isEmpty()) {
		restoreState(settings.lastWindowState());
	} else {
		initDefaultDocks();
	}

	if(const auto lastWindowViewState = settings.lastWindowViewState(); !lastWindowViewState.isEmpty()) {
		m_splitter->restoreState(settings.lastWindowViewState());
		m_splitter->setHandleWidth(m_splitterOriginalHandleWidth);
	}

	const auto docksConfig = settings.lastWindowDocks();
	for(auto *dw : findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if(!dw->objectName().isEmpty()) {
			const auto dock = docksConfig.value(dw->objectName()).value<QVariantMap>();
			if(dock.value("undockable", false).toBool()) {
				dw->setFloating(true);
				dw->setAllowedAreas(Qt::NoDockWidgetArea);
			}
		}
	}
}

void MainWindow::initSmallScreenState()
{
	initDefaultDocks();
	for(QDockWidget *dw : findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		dw->hide();
	}
	m_splitter->setHandleWidth(0);
	m_chatbox->hide();
	m_toolBarDraw->show();
}

void MainWindow::initDefaultDocks()
{
	// More event loop flushing to get Qt to *actually* apply these sizes. It
	// gets horribly confused by hidden docks and the event loop must be woken
	// up numerous times to actually apply the sizes. This arrangement works,
	// I'd recommend not messing with it unless there's actually an issue.
	setDefaultDockSizes();
	dpApp().processEvents();
	m_dockTimeline->hide();
	m_dockOnionSkins->hide();
	dpApp().processEvents();
	setDefaultDockSizes();
	dpApp().processEvents();
}

void MainWindow::setDefaultDockSizes()
{
	int leftWidth = 320, leftHeight = 220;
	int rightWidth = 260, rightHeight = 200;
	int topHeight = 300;
	resizeDocks(
		{m_dockToolSettings, m_dockBrushPalette, m_dockColorSpinner,
			m_dockColorSliders, m_dockColorPalette, m_dockLayers},
		{leftWidth, leftWidth, rightWidth, rightWidth, rightWidth,
			rightWidth},
		Qt::Horizontal);
	resizeDocks(
		{m_dockToolSettings, m_dockColorSpinner, m_dockColorSliders,
			m_dockColorPalette, m_dockTimeline, m_dockOnionSkins},
		{leftHeight, rightHeight, rightHeight, rightHeight, topHeight,
			topHeight},
		Qt::Vertical);
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
	switch (event->type()) {
	// This is a guessed list of events that might cause the QMainWindow state
	// to change, and it seems to work OK but may be wrong or excessive
	case QEvent::Show:
	case QEvent::Hide:
	case QEvent::Move:
	case QEvent::Resize:
	case QEvent::Close:
		m_saveWindowDebounce.start();
		break;
	case QEvent::Shortcut: {
		QShortcutEvent *shortcutEvent = static_cast<QShortcutEvent *>(event);
		if(shortcutEvent->isAmbiguous()) {
			handleAmbiguousShortcut(shortcutEvent);
			return true;
		}
		break;
	}
	default: {}
	}
	return QMainWindow::eventFilter(object, event);
}

void MainWindow::handleAmbiguousShortcut(QShortcutEvent *shortcutEvent)
{
	CustomShortcutModel shortcutsModel;
	shortcutsModel.loadShortcuts(dpApp().settings().shortcuts());

	const QKeySequence &keySequence = shortcutEvent->key();
	QVector<CustomShortcut> shortcuts = shortcutsModel.getShortcutsMatching(keySequence);
	// Shortcuts may conflict with stuff like the main window menu bar. We can
	// resolve those pseudo.conflicts in the favor of our custom shortcuts.
	if(shortcuts.size() == 1) {
		QAction *action = findChild<QAction*>(
			shortcuts.first().name, Qt::FindDirectChildrenOnly);
		if(action) {
			action->trigger();
			return;
		}
	}

	QStringList matchingShortcuts;
	for(const CustomShortcut &shortcut : shortcuts) {
		matchingShortcuts.append(QString("<li>%1</li>").arg(shortcut.title.toHtmlEscaped()));
	}
	matchingShortcuts.sort(Qt::CaseInsensitive);

	QString message =
		tr("<p>The shortcut '%1' is ambiguous, it matches:</p><ul>%2</ul>")
			.arg(keySequence.toString(QKeySequence::NativeText))
			.arg(matchingShortcuts.join(QString()));

	QMessageBox box{
		QMessageBox::Warning, tr("Ambiguous Shortcut"), message,
		QMessageBox::Close, this};

	QPushButton *fixButton = box.addButton(tr("Fix"), QMessageBox::ActionRole);

	box.exec();
	if(box.clickedButton() == fixButton) {
		showSettings()->activateShortcutsPanel();
	}
}

void MainWindow::saveSplitterState()
{
	if(!updatesEnabled()) {
		m_saveSplitterDebounce.start();
		return;
	}
	m_saveSplitterDebounce.stop();
	if(!m_smallScreenMode) {
		auto &settings = dpApp().settings();
		settings.setLastWindowViewState(m_splitter->saveState());
	}
}

void MainWindow::saveWindowState()
{
	if(!updatesEnabled()) {
		m_saveWindowDebounce.start();
		return;
	}
	m_saveWindowDebounce.stop();

	auto &settings = dpApp().settings();
	settings.setLastWindowPosition(normalGeometry().topLeft());
	settings.setLastWindowSize(normalGeometry().size());
	if(!m_smallScreenMode) {
		settings.setLastWindowMaximized(isMaximized());
		settings.setLastWindowState(m_hiddenDockState.isEmpty() ? saveState() : m_hiddenDockState);

		// TODO: This should be separate from window state and happen only when dock
		// states change
		Settings::LastWindowDocksType docksConfig;
		for (const auto *dw : findChildren<const QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
			if(!dw->objectName().isEmpty()) {
				docksConfig[dw->objectName()] = QVariantMap {
					{"undockable", dw->isFloating() && dw->allowedAreas() == Qt::NoDockWidgetArea}
				};
			}
		}
		settings.setLastWindowDocks(docksConfig);
	}

	m_dockToolSettings->saveSettings();
}

void MainWindow::requestUserInfo(int userId)
{
	net::Client *client = m_doc->client();
	QJsonObject info{{"type", "request_user_info"}};
	client->sendMessage(net::makeUserInfoMessage(
		client->myId(), userId, QJsonDocument{info}));
}

void MainWindow::sendUserInfo(int userId)
{
	// Android reports "linux" as the kernel type, which is not helpful.
#if defined(Q_OS_ANDROID)
	QString os = QSysInfo::productType();
#else
	QString os = QSysInfo::kernelType();
#endif
	QJsonObject info{
		{"type", "user_info"},
		{"app_version", QStringLiteral("%1 (%2)").arg(
			cmake_config::version(), QSysInfo::buildCpuArchitecture())},
		{"protocol_version", DP_PROTOCOL_VERSION},
		{"qt_version", QString::number(QT_VERSION_MAJOR)},
		{"os", os},
		{"tablet_input", tabletinput::current()},
		{"tablet_mode", m_canvasView->isTabletEnabled() ? "pressure" : "none"},
		{"touch_mode", m_canvasView->isTouchDrawEnabled() ? "draw"
			: m_canvasView->isTouchScrollEnabled() ? "scroll" : "none"},
		{"smoothing", m_doc->toolCtrl()->globalSmoothing()},
		{"pressure_curve", m_canvasView->pressureCurveAsString()},
	};
	net::Client *client = m_doc->client();
	client->sendMessage(net::makeUserInfoMessage(
		client->myId(), userId, QJsonDocument{info}));
}

void MainWindow::requestCurrentBrush(int userId)
{
	m_brushRequestUserId = userId;
	m_brushRequestCorrelator = QUuid::createUuid().toString();
	m_brushRequestTime.start();
	QJsonObject info{
		{QStringLiteral("type"), QStringLiteral("request_current_brush")},
		{QStringLiteral("correlator"), m_brushRequestCorrelator},
	};
	net::Client *client = m_doc->client();
	client->sendMessage(net::makeUserInfoMessage(
		client->myId(), userId, QJsonDocument{info}));
}

void MainWindow::sendCurrentBrush(int userId, const QString &correlator)
{
	tools::BrushSettings *bs = m_dockToolSettings->brushSettings();
	QJsonObject info = {
		{QStringLiteral("type"), QStringLiteral("current_brush")},
		{QStringLiteral("brush"), bs->currentBrush().toShareJson()},
		{QStringLiteral("correlator"), correlator},
	};
	net::Client *client = m_doc->client();
	client->sendMessage(net::makeUserInfoMessage(
		client->myId(), userId, QJsonDocument{info}));
}

void MainWindow::receiveCurrentBrush(int userId, const QJsonObject &info)
{
	bool wasRequested = m_brushRequestUserId == userId &&
						m_brushRequestCorrelator ==
							info[QStringLiteral("correlator")].toString() &&
						m_brushRequestTime.isValid() &&
						!m_brushRequestTime.hasExpired(30000);
	if(wasRequested) {
		m_brushRequestUserId = -1;
		m_brushRequestCorrelator.clear();
		m_brushRequestTime.invalidate();
		QJsonValue v = info[QStringLiteral("brush")];
		if(v.isObject()) {
			tools::BrushSettings *bs = m_dockToolSettings->brushSettings();
			bs->setCurrentBrush(brushes::ActiveBrush::fromJson(v.toObject()));
		}
	}
}

void MainWindow::fillArea(const QColor &color, int blendMode, float opacity)
{
	if(blendMode != DP_BLEND_MODE_ERASE) {
		m_dockToolSettings->addLastUsedColor(color);
	}
	m_doc->fillArea(color, DP_BlendMode(blendMode), opacity);
}

void MainWindow::fillAreaWithBlendMode(int blendMode)
{
	fillArea(m_dockToolSettings->foregroundColor(), blendMode, 1.0f);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	updateInterfaceMode();
}

/**
 * Confirm exit. A confirmation dialog is popped up if there are unsaved
 * changes or network connection is open.
 * @param event event info
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
#ifdef __EMSCRIPTEN__
	event->ignore();
#else
	QApplication::restoreOverrideCursor();
	setEnabled(true);

	if(m_doc->isSaveInProgress()) {
		// Don't quit while save is in progress
		m_exitAction = SAVING;
		event->ignore();
		return;
	}

	if(canReplace() == false) {

		// First confirm disconnection
		if(m_doc->client()->isLoggedIn()) {
			QMessageBox box(
				QMessageBox::Information,
				tr("Exit Drawpile"),
				tr("You are still connected to a drawing session."),
				QMessageBox::NoButton, this);
#ifndef __EMSCRIPTEN__
			box.setWindowModality(Qt::WindowModal);
#endif

			const QPushButton *exitbtn = box.addButton(tr("Exit anyway"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Cancel"),
					QMessageBox::RejectRole);

			box.exec();
			if(box.clickedButton() == exitbtn) {
				// Disconnect and wait a moment for things to settle so that
				// e.g. the builtin server gets a chance to shut down properly
				// and any pending drawing commands get executed.
				m_exitAction = DISCONNECTING;
				m_doc->client()->disconnectFromServer();
				setEnabled(false);
				QApplication::setOverrideCursor(Qt::WaitCursor);
			}
			event->ignore();
			return;
		}

		// Then confirm unsaved changes
		if(isWindowModified()) {
			QMessageBox box(QMessageBox::Question, tr("Exit Drawpile"),
					tr("There are unsaved changes. Save them before exiting?"),
					QMessageBox::NoButton, this);
#ifndef __EMSCRIPTEN__
			box.setWindowModality(Qt::WindowModal);
#endif
			const QPushButton *savebtn = box.addButton(tr("Save"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Discard"),
					QMessageBox::DestructiveRole);
			const QPushButton *cancelbtn = box.addButton(tr("Cancel"),
					QMessageBox::RejectRole);

			box.exec();
			bool cancel = false;
			// Save and exit, or cancel exit if couldn't save.
			if(box.clickedButton() == savebtn) {
				cancel = true;
				if(save()) {
					m_exitAction = SAVING;
				}
			}

			// Cancel exit
			if(box.clickedButton() == cancelbtn || cancel) {
				event->ignore();
				return;
			}
		}
	}

	exit();
#endif
}

bool MainWindow::event(QEvent *event)
{
	switch (event->type()) {
	case QEvent::StatusTip:
		m_viewStatusBar->showMessage(static_cast<QStatusTipEvent*>(event)->tip());
		return true;
	case QEvent::KeyRelease:
		// Monitor key-up events to switch back from temporary tools/tool slots.
		// A short tap of the tool switch shortcut switches the tool permanently as usual,
		// but when holding it down, the tool is activated just temporarily. The
		// previous tool be switched back automatically when the shortcut key is released.
		// Note: for simplicity, we only support tools with single key shortcuts.
		if (m_toolChangeTime.elapsed() > 250) {
			const QKeyEvent *e = static_cast<const QKeyEvent*>(event);
			if(!e->isAutoRepeat()) {
				if(m_tempToolSwitchShortcut->isShortcutSent() && e->modifiers() == Qt::NoModifier) {
						// Return from temporary tool change
						for(const QAction *act : m_drawingtools->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && compat::keyPressed(*e) == seq[0]) {
								m_dockToolSettings->setPreviousTool();
								break;
							}
						}

						// Return from temporary tool slot change
						for(const QAction *act : m_brushSlots->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && compat::keyPressed(*e) == seq[0]) {
								m_dockToolSettings->setPreviousTool();
								break;
						}
					}
				}

				m_tempToolSwitchShortcut->reset();
			}
		}
		break;
	case QEvent::ShortcutOverride: {
			// QLineEdit doesn't seem to override the Return key shortcut,
			// so we have to do it ourself.
			const QKeyEvent *e = static_cast<QKeyEvent*>(event);
			if(e->key() == Qt::Key_Return) {
				QWidget *focus = QApplication::focusWidget();
				if(focus && focus->inherits("QLineEdit")) {
					event->accept();
					return true;
				}
			}
		}
	break;
	case QEvent::Move:
	case QEvent::Resize:
	case QEvent::WindowStateChange:
		m_saveWindowDebounce.start();
		break;
	case QEvent::ActivationChange:
		if(m_saveSplitterDebounce.isActive()) {
			saveSplitterState();
		}
		if(m_saveWindowDebounce.isActive()) {
			saveWindowState();
		}
		m_canvasView->clearKeys();
		dpApp().settings().trySubmit();
		break;
	default:
		break;
	}

	return QMainWindow::event(event);
}

dialogs::StartDialog *MainWindow::showStartDialog()
{
	dialogs::StartDialog *dlg =
		new dialogs::StartDialog(m_smallScreenMode, this);
	dlg->setObjectName(QStringLiteral("startdialog"));
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connectStartDialog(dlg);
	utils::showWindow(dlg, shouldShowDialogMaximized());
	return dlg;
}

void MainWindow::showPopupMessage(const QString &message)
{
	m_netstatus->showMessage(message);
}

void MainWindow::connectStartDialog(dialogs::StartDialog *dlg)
{
	static constexpr char key[] = "startdialogconnections";
	class Connections final : public QObject {
	public:
		Connections(QObject *parent)
			: QObject{parent}
		{
			setObjectName(key);
		}

		void add(const QMetaObject::Connection &con)
		{
			m_values.append(con);
		}

		void clear()
		{
			for(const QMetaObject::Connection &con : m_values) {
				disconnect(con);
			}
		}

	private:
		QVector<QMetaObject::Connection> m_values;
	};

	Connections *previousConnections = dlg->findChild<Connections *>(key);
	if(previousConnections) {
		previousConnections->clear();
		delete previousConnections;
	}

	Connections *connections = new Connections{dlg};
	connections->add(connect(dlg, &dialogs::StartDialog::openFile, this, &MainWindow::open));
	connections->add(connect(dlg, &dialogs::StartDialog::openPath, this, std::bind(&MainWindow::openPath, this, _1, nullptr)));
	connections->add(connect(dlg, &dialogs::StartDialog::layouts, this, &MainWindow::showLayoutsDialog));
	connections->add(connect(dlg, &dialogs::StartDialog::preferences, this, &MainWindow::showSettings));
	connections->add(connect(dlg, &dialogs::StartDialog::join, this, &MainWindow::joinSession));
	connections->add(connect(dlg, &dialogs::StartDialog::host, this, &MainWindow::hostSession));
	connections->add(connect(dlg, &dialogs::StartDialog::create, this, &MainWindow::newDocument));
	connections->add(connect(m_doc, &Document::canvasChanged, dlg, std::bind(&MainWindow::closeStartDialog, this, dlg, true)));
	connections->add(connect(m_doc, &Document::serverLoggedIn, dlg, std::bind(&MainWindow::closeStartDialog, this, dlg, _1)));
	connections->add(connect(this, &MainWindow::hostSessionEnabled, dlg, &dialogs::StartDialog::hostPageEnabled));
	connections->add(connect(this, &MainWindow::windowReplacementFailed, dlg, [dlg](MainWindow *win){
		if(win) {
			dlg->setParent(win, dlg->windowFlags());
			win->connectStartDialog(dlg);
			utils::showWindow(dlg, win->shouldShowDialogMaximized());
		} else {
			dlg->deleteLater();
		}
	}));
	setStartDialogActions(dlg);
}

void MainWindow::setStartDialogActions(dialogs::StartDialog *dlg)
{
	QPair<dialogs::StartDialog::Entry, const char *> pairs[] = {
		{dialogs::StartDialog::Entry::Join, "joinsession"},
		{dialogs::StartDialog::Entry::Browse, "browsesession"},
		{dialogs::StartDialog::Entry::Host, "hostsession"},
		{dialogs::StartDialog::Entry::Create, "newdocument"},
		{dialogs::StartDialog::Entry::Open, "opendocument"},
		{dialogs::StartDialog::Entry::Layouts, "layouts"},
		{dialogs::StartDialog::Entry::Preferences, "preferences"},
	};
	dialogs::StartDialog::Actions actions{};
	for(const auto &[entry, action] : pairs) {
		actions.entries[entry] = getAction(action);
	}
	dlg->setActions(actions);
}

void MainWindow::closeStartDialog(dialogs::StartDialog *dlg, bool reparent)
{
	if(reparent) {
		for(QDialog *child : dlg->findChildren<QDialog *>(
				QString(), Qt::FindDirectChildrenOnly)) {
			child->setParent(this, child->windowFlags());
			child->show();
		}
	} else {
	}
	dlg->close();
}

QWidget *MainWindow::getStartDialogOrThis()
{
	dialogs::StartDialog *dlg = findChild<dialogs::StartDialog *>(
		QStringLiteral("startdialog"), Qt::FindDirectChildrenOnly);
	if(dlg) {
		return dlg;
	} else {
		return this;
	}
}

void MainWindow::start()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Entry::Guess);
}

/**
 * Show the "new document" dialog
 */
void MainWindow::showNew()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Entry::Create);
}

void MainWindow::newDocument(const QSize &size, const QColor &background)
{
	if(canReplace()) {
		m_doc->loadBlank(size, background);
	} else {
		prepareWindowReplacement();
		bool newProcessStarted = dpApp().runInNewProcess(
			{QStringLiteral("--no-restore-window-position"),
			 QStringLiteral("--blank"),
			 QStringLiteral("%1x%2x%3")
				 .arg(size.width())
				 .arg(size.height())
				 .arg(background.name(QColor::HexArgb).remove('#'))});
		if(newProcessStarted) {
			emit windowReplacementFailed(nullptr);
		} else {
			MainWindow *win = new MainWindow(false);
			emit windowReplacementFailed(win);
			win->m_doc->loadBlank(size, background);
		}
	}
}

void MainWindow::openPath(const QString &path, QTemporaryFile *tempFile)
{
	if(!canReplace()) {
		prepareWindowReplacement();
		bool newProcessStarted = dpApp().runInNewProcess(
			{QStringLiteral("--no-restore-window-position"),
			 QStringLiteral("--open"), path});
		if(newProcessStarted) {
			emit windowReplacementFailed(nullptr);
			// The temporary file is only used in the browser, which will never
			// start new processes, so it really should always be null here.
			Q_ASSERT(!tempFile);
			delete tempFile;
		} else {
			MainWindow *win = new MainWindow(false);
			emit windowReplacementFailed(win);
			win->openPath(path, tempFile);
		}
		return;
	}

	QString loadPath = tempFile ? tempFile->fileName() : path;
	static constexpr auto opt = QRegularExpression::CaseInsensitiveOption;
	if(QRegularExpression{"\\.dp(rec|txt)$", opt}.match(path).hasMatch()) {
		bool isTemplate;
		DP_LoadResult result = m_doc->loadRecording(loadPath, false, &isTemplate);
		showLoadResultMessage(result);
		if(result == DP_LOAD_RESULT_SUCCESS && !isTemplate) {
			QFileInfo fileinfo(path);
			m_playbackDialog = new dialogs::PlaybackDialog(m_doc->canvas(), this);
			m_playbackDialog->setWindowTitle(fileinfo.completeBaseName() + " - " + m_playbackDialog->windowTitle());
			m_playbackDialog->setAttribute(Qt::WA_DeleteOnClose);
			m_playbackDialog->show();
			m_playbackDialog->centerOnParent();
			if(tempFile) {
				tempFile->setParent(m_playbackDialog);
			}
			connect(
				m_playbackDialog, &dialogs::PlaybackDialog::playbackToggled,
				this, &MainWindow::setRecorderStatus);
			connect(
				m_playbackDialog, &dialogs::PlaybackDialog::destroyed, this,
				[this](QObject *) {
					m_playbackDialog = nullptr;
					setRecorderStatus(false);
				});
		} else {
			delete tempFile;
		}

	} else if(QRegularExpression{"\\.drawdancedump$", opt}.match(path).hasMatch()) {
		DP_LoadResult result = m_doc->loadRecording(loadPath, true);
		if(result == DP_LOAD_RESULT_SUCCESS) {
			QFileInfo fileinfo{path};
			m_dumpPlaybackDialog = new dialogs::DumpPlaybackDialog{m_doc->canvas(), this};
			m_dumpPlaybackDialog->setWindowTitle(QStringLiteral("%1 - %2")
				.arg(fileinfo.completeBaseName()).arg(m_dumpPlaybackDialog->windowTitle()));
			m_dumpPlaybackDialog->setAttribute(Qt::WA_DeleteOnClose);
			m_dumpPlaybackDialog->show();
			if(tempFile) {
				tempFile->setParent(m_dumpPlaybackDialog);
			}
		} else {
			delete tempFile;
		}

	} else {
		QApplication::setOverrideCursor(Qt::WaitCursor);
		QProgressDialog *progressDialog = new QProgressDialog(this);
		progressDialog->setRange(0, 0);
		progressDialog->setMinimumDuration(500);
		progressDialog->setCancelButton(nullptr);
		progressDialog->setLabelText(tr("Opening file…"));
		setEnabled(false);

		CanvasLoaderRunnable *loader = new CanvasLoaderRunnable(loadPath, this);
		loader->setAutoDelete(false);
		connect(
			loader, &CanvasLoaderRunnable::loadComplete, this,
			[=](const QString &error, const QString &detail) {
				delete tempFile;
				setEnabled(true);
				delete progressDialog;
				QApplication::restoreOverrideCursor();

				const drawdance::CanvasState &canvasState =
					loader->canvasState();
				if(canvasState.isNull()) {
					showErrorMessageWithDetails(error, detail);
				} else {
					m_doc->loadState(
						canvasState, loader->path(), loader->type(), false);
				}

				loader->deleteLater();
			}, Qt::QueuedConnection);

		QThreadPool::globalInstance()->start(loader);
	}

	addRecentFile(path);
}

/**
 * Show a file selector dialog. If there are unsaved changes, open the file
 * in a new window.
 */
void MainWindow::open()
{
	FileWrangler(getStartDialogOrThis()).openMain(
		std::bind(&MainWindow::openPath, this, _1, _2));
}

#ifdef __EMSCRIPTEN__
void MainWindow::download()
{
	FileWrangler(this).downloadImage(m_doc);
}

void MainWindow::downloadSelection()
{
	FileWrangler(this).downloadSelection(m_doc);
}
#else
/**
 * If no file name has been selected, \a saveas is called.
 */
bool MainWindow::save()
{
	QString result = FileWrangler{this}.saveImage(m_doc);
	if(result.isEmpty()) {
		return false;
	} else {
		addRecentFile(result);
		return true;
	}
}

/**
 * A standard file dialog is used to get the name of the file to save.
 * If no suffix is the suffix from the current filter is used.
 */
void MainWindow::saveas()
{
	QString result = FileWrangler{this}.saveImageAs(m_doc, false);
	if(!result.isEmpty()) {
		addRecentFile(result);
	}
}

void MainWindow::saveSelection()
{
	QString result = FileWrangler{this}.saveSelectionAs(m_doc);
	if(!result.isEmpty()) {
		addRecentFile(result);
	}
}

void MainWindow::exportImage()
{
	QString result = FileWrangler(this).saveImageAs(m_doc, true);
	if(!result.isEmpty()) {
		addRecentFile(result);
	}
}
#endif

void MainWindow::importOldAnimation()
{
	// If we're on a single-window system, don't clobber that window just to
	// show the dialog. Otherwise a single mis-click could obliterate work.
#ifdef SINGLE_MAIN_WINDOW
	bool showDialogNow = true;
#else
	bool showDialogNow = canReplace();
#endif
	if(showDialogNow) {
		dialogs::AnimationImportDialog *dlg =
			new dialogs::AnimationImportDialog(this);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		connect(
			dlg, &dialogs::AnimationImportDialog::canvasStateImported, this,
			[this, dlg](const drawdance::CanvasState &canvasState) {
				MainWindow *win;
				if(canReplace()) {
					win = this;
				} else {
					prepareWindowReplacement();
					win = new MainWindow;
					emit windowReplacementFailed(win);
				}
				// Don't use the path of the imported animation to avoid
				// clobbering of the old file by mashing Ctrl+S instinctually.
				win->m_doc->loadState(
					canvasState, QString(), DP_SAVE_IMAGE_UNKNOWN, true);
				dlg->deleteLater();
			});
		utils::showWindow(dlg, shouldShowDialogMaximized());
	} else {
		prepareWindowReplacement();
		bool newProcessStarted = dpApp().runInNewProcess(
			{QStringLiteral("--no-restore-window-position"),
			 QStringLiteral("--start-page"),
			 QStringLiteral("import-old-animation")});
		if(newProcessStarted) {
			emit windowReplacementFailed(nullptr);
		} else {
			MainWindow *win = new MainWindow(false);
			emit windowReplacementFailed(win);
			win->importOldAnimation();
		}
	}
}

void MainWindow::onCanvasSaveStarted()
{
	QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
#ifdef __EMSCRIPTEN__
	getAction("downloaddocument")->setEnabled(false);
	getAction("downloadselection")->setEnabled(false);
#else
	getAction("savedocument")->setEnabled(false);
	getAction("savedocumentas")->setEnabled(false);
	getAction("exportdocument")->setEnabled(false);
#endif
	m_viewStatusBar->showMessage(tr("Saving..."));
	m_canvasView->setSaveInProgress(true);
}

void MainWindow::onCanvasSaved(const QString &errorMessage)
{
	QApplication::restoreOverrideCursor();
#ifdef __EMSCRIPTEN__
	getAction("downloaddocument")->setEnabled(false);
	getAction("downloadselection")->setEnabled(false);
#else
	getAction("savedocument")->setEnabled(true);
	getAction("savedocumentas")->setEnabled(true);
	getAction("exportdocument")->setEnabled(true);
#endif
	m_canvasView->setSaveInProgress(false);

	setWindowModified(m_doc->isDirty());
	updateTitle();

	if(!errorMessage.isEmpty()) {
		m_viewStatusBar->showMessage(tr("Image saving failed"), 1000);
		showErrorMessageWithDetails(tr("Couldn't save image"), errorMessage);
	} else {
		m_viewStatusBar->showMessage(tr("Image saved"), 1000);
	}

#ifndef __EMSCRIPTEN__
	// Cancel exit if canvas is modified while it was being saved
	if(m_doc->isDirty())
		m_exitAction = RUNNING;

	if(m_exitAction == SAVING)
		close();
#endif
}

#ifdef __EMSCRIPTEN__
void MainWindow::onCanvasDownloadStarted()
{
	onCanvasSaveStarted();
}

void MainWindow::onCanvasDownloadReady(const QString &defaultName, const QByteArray &bytes)
{
	onCanvasSaved(QString());
	QMessageBox *msgbox = utils::makeInformationWithSaveButton(
		this, tr("Download Complete"),
		tr("Download complete, click on \"Save\" to save your file."));
	connect(
		msgbox->button(QMessageBox::Save), &QAbstractButton::clicked, this,
		[this, defaultName, bytes]() {
			FileWrangler(this).saveFileContent(defaultName, bytes);
		});
	msgbox->show();
}

void MainWindow::onCanvasDownloadError(const QString &errorMessage)
{
	onCanvasSaved(errorMessage);
}
#endif

void MainWindow::showResetNoticeDialog(const drawdance::CanvasState &canvasState)
{
	m_canvasView->setCatchupProgress(0, true);
	m_canvasView->showResetNotice(
		m_doc->isCompatibilityMode(), m_doc->isSaveInProgress());
	if(m_preResetCanvasState.isNull()) {
		m_preResetCanvasState = canvasState;
	}
}

void MainWindow::updateCatchupProgress(int percent)
{
	if(percent >= 100 && m_initialCatchup) {
		m_initialCatchup = false;
		dpApp().notifications()->trigger(
			this, notification::Event::Login, tr("Joined the session!"));
	}
	m_canvasView->setCatchupProgress(percent, false);
}

void MainWindow::savePreResetImageAs()
{
	if(m_preResetCanvasState.isNull()) {
		m_canvasView->hideResetNotice();
	} else {
#ifdef __EMSCRIPTEN__
		if(FileWrangler(this).downloadPreResetImage(m_doc, m_preResetCanvasState)) {
			m_canvasView->hideResetNotice();
		}
#else
		QString result = FileWrangler(this).savePreResetImageAs(
			m_doc, m_preResetCanvasState);
		if(!result.isEmpty()) {
			m_preResetCanvasState = drawdance::CanvasState::null();
			m_canvasView->hideResetNotice();
			addRecentFile(result);
		}
#endif
	}
}

void MainWindow::discardPreResetImage()
{
	m_preResetCanvasState = drawdance::CanvasState::null();
	m_canvasView->hideResetNotice();
}

void MainWindow::showCompatibilityModeWarning()
{
	if(m_doc->isCompatibilityMode()) {
		QString message = tr(
			"This session was hosted with an older version of Drawpile, some "
			"newer features won't be available. Other Drawpile versions will "
			"see different results, session resets may cause abrupt changes.");
		QMessageBox *box = new QMessageBox{
			QMessageBox::Warning, tr("Compatibility Mode"), message,
			QMessageBox::Ok, this};
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->setModal(false);
		box->show();
	}
}

void MainWindow::exportTemplate()
{
	QString filename = FileWrangler{this}.getSaveTemplatePath();
	if(!filename.isEmpty()) {
		m_doc->exportTemplate(filename);
	}
}

void MainWindow::onTemplateExported(const QString &errorMessage)
{
	if(errorMessage.isEmpty()) {
		m_viewStatusBar->showMessage(tr("Session template saved"), 1000);
	} else {
		showErrorMessageWithDetails(tr("Couldn't export session template"), errorMessage);
	}
}

void MainWindow::exportGifAnimation()
{
	exportGifAnimationWith(
		m_doc->canvas()->paintEngine()->viewCanvasState(), QRect{}, -1, -1, -1);
}

void MainWindow::exportGifAnimationWith(
	const drawdance::CanvasState &canvasState, const QRect &crop, int start,
	int end, int framerate)
{
	QString filename = FileWrangler{this}.getSaveGifPath();
	if(!filename.isEmpty()) {
		exportAnimation(
			canvasState, filename,
			[=](DP_CanvasState *cs, const char *path, DP_SaveAnimationProgressFn progress, void *user){
				DP_Rect r, *pr;
				if(crop.isEmpty()) {
					pr = nullptr;
				} else {
					r = DP_rect_make(
						crop.x(), crop.y(), crop.width(), crop.height());
					pr = &r;
				}
				return DP_save_animation_gif(
					cs, path, pr, start, end, framerate, progress, user);
			});
	}
}

#ifndef Q_OS_ANDROID
void MainWindow::exportAnimationFrames()
{
	exportAnimationFramesWith(
		m_doc->canvas()->paintEngine()->viewCanvasState(), QRect{}, -1, -1);
}

void MainWindow::exportAnimationFramesWith(
	const drawdance::CanvasState &canvasState, const QRect &crop, int start,
	int end)
{
	QString dirname = FileWrangler{this}.getSaveAnimationFramesPath();
	if(!dirname.isEmpty()) {
		exportAnimation(
			canvasState, dirname,
			[=](DP_CanvasState *cs, const char *path, DP_SaveAnimationProgressFn progress, void *user){
				DP_Rect r, *pr;
				if(crop.isEmpty()) {
					pr = nullptr;
				} else {
					r = DP_rect_make(
						crop.x(), crop.y(), crop.width(), crop.height());
					pr = &r;
				}
				return DP_save_animation_frames(cs, path, pr, start, end, progress, user);
			});
	}
}
#endif

void MainWindow::exportAnimation(
	const drawdance::CanvasState &canvasState, const QString &path,
	AnimationSaverRunnable::SaveFn saveFn)
{
	auto *progressDialog = new QProgressDialog(
		tr("Saving animation..."),
		tr("Cancel"),
		0,
		100,
		this);
	progressDialog->setMinimumDuration(500);

	AnimationSaverRunnable *saver =
		new AnimationSaverRunnable(canvasState, saveFn, path);

	connect(saver, &AnimationSaverRunnable::progress, progressDialog, &QProgressDialog::setValue);
	connect(saver, &AnimationSaverRunnable::saveComplete, this, &MainWindow::showErrorMessage);
	connect(progressDialog, &QProgressDialog::canceled, saver, &AnimationSaverRunnable::cancelExport);

	progressDialog->setValue(0);
	QThreadPool::globalInstance()->start(saver);
}

void MainWindow::showFlipbook()
{
	dialogs::Flipbook *fp = findChild<dialogs::Flipbook *>(
		"flipbook", Qt::FindDirectChildrenOnly);
	if(fp) {
		fp->setPaintEngine(m_doc->canvas()->paintEngine());
		fp->activateWindow();
		fp->raise();
	} else {
		fp = new dialogs::Flipbook{m_flipbookState, this};
		fp->setObjectName("flipbook");
		fp->setAttribute(Qt::WA_DeleteOnClose);
		canvas::CanvasModel *canvas = m_doc->canvas();
		canvas::SelectionModel *sel = canvas->selection();
		fp->setPaintEngine(
			canvas->paintEngine(), sel->isValid() ? sel->bounds() : QRect());
		fp->setRefreshShortcuts(getAction("showflipbook")->shortcuts());
		connect(
			fp, &dialogs::Flipbook::exportGifRequested, this,
			&MainWindow::exportGifAnimationWith);
#ifndef Q_OS_ANDROID
		connect(
			fp, &dialogs::Flipbook::exportFramesRequested, this,
			&MainWindow::exportAnimationFramesWith);
#endif
		utils::showWindow(fp, shouldShowDialogMaximized());
	}
}

void MainWindow::setRecorderStatus(bool on)
{
#ifdef __EMSCRIPTEN__
	Q_UNUSED(on);
#else
	QAction *recordAction = getAction("recordsession");

	if(m_playbackDialog) {
		if(m_playbackDialog->isPlaying()) {
			recordAction->setIcon(QIcon::fromTheme("media-playback-pause"));
			recordAction->setText(tr("Pause"));
		} else {
			recordAction->setIcon(QIcon::fromTheme("media-playback-start"));
			recordAction->setText(tr("Play"));
		}

	} else {
		if(on) {
			recordAction->setText(tr("Stop Recording"));
			recordAction->setIcon(QIcon::fromTheme("media-playback-stop"));
		} else {
			recordAction->setText(tr("Record..."));
			recordAction->setIcon(QIcon::fromTheme("media-record"));
		}
	}
#endif
}

void MainWindow::showSystemInfo()
{
	dialogs::SystemInfoDialog *dlg = findChild<dialogs::SystemInfoDialog *>(
		"systeminfodialog", Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->setParent(getStartDialogOrThis());
	} else {
		dlg = new dialogs::SystemInfoDialog(this);
		dlg->setObjectName("systeminfodialog");
		dlg->setAttribute(Qt::WA_DeleteOnClose);
	}
	utils::showWindow(dlg, shouldShowDialogMaximized());
	dlg->activateWindow();
	dlg->raise();
}

void MainWindow::toggleRecording()
{
	if(m_playbackDialog) {
		// If the playback dialog is visible, this action works as the play/pause button
		m_playbackDialog->setPlaying(!m_playbackDialog->isPlaying());
		return;
	}

	if(m_doc->stopRecording()) {
		return; // There was a recording and we just stopped it.
	}

	QString filename = FileWrangler{this}.getSaveRecordingPath();
	if(!filename.isEmpty()) {
		drawdance::RecordStartResult result = m_doc->startRecording(filename);
		switch(result) {
		case drawdance::RECORD_START_SUCCESS:
			break;
		case drawdance::RECORD_START_UNKNOWN_FORMAT:
			showErrorMessage(tr("Unsupported format."));
			break;
		case drawdance::RECORD_START_OPEN_ERROR:
			showErrorMessageWithDetails(tr("Couldn't start recording."), DP_error());
			break;
		default:
			showErrorMessageWithDetails(tr("Unknown error."), DP_error());
			break;
		}
	}
}

void MainWindow::toggleProfile()
{
#ifdef __EMSCRIPTEN__
	QString path = QStringLiteral("/profile.dpperf");
#endif
	if(drawdance::Perf::isOpen()) {
		if(drawdance::Perf::close()) {
#ifdef __EMSCRIPTEN__
			QFile f(path);
			if(f.open(QIODevice::ReadOnly)) {
				FileWrangler(this).saveFileContent(
					QStringLiteral("profile%1.dpperf")
						.arg(QDateTime::currentSecsSinceEpoch()),
					f.readAll());
			} else {
				showErrorMessageWithDetails(
					tr("Error downloading profile."), f.errorString());
			}
			f.remove();
#endif
		} else {
			showErrorMessageWithDetails(tr("Error closing profile."), DP_error());
		}
	} else {
#ifndef __EMSCRIPTEN__
		QString path = FileWrangler{this}.getSavePerformanceProfilePath();
#endif
		if(!path.isEmpty()) {
			if(!drawdance::Perf::open(path)) {
				showErrorMessageWithDetails(tr("Error opening profile."), DP_error());
			}
		}
	}
}

void MainWindow::toggleTabletEventLog()
{
#ifdef __EMSCRIPTEN__
	QString path = QStringLiteral("/eventlog.dplog");
#endif
	if(drawdance::EventLog::isOpen()) {
		if(drawdance::EventLog::close()) {
#ifdef __EMSCRIPTEN__
			QFile f(path);
			if(f.open(QIODevice::ReadOnly)) {
				FileWrangler(this).saveFileContent(
					QStringLiteral("eventlog%1.dplog")
						.arg(QDateTime::currentSecsSinceEpoch()),
					f.readAll());
			} else {
				showErrorMessageWithDetails(
					tr("Error downloading tablet event log."), f.errorString());
			}
			f.remove();
#endif
		} else {
			showErrorMessageWithDetails(tr("Error closing tablet event log."), DP_error());
		}
	} else {
#ifndef __EMSCRIPTEN__
		QString path = FileWrangler{this}.getSaveTabletEventLogPath();
#endif
		if(!path.isEmpty()) {
			if(drawdance::EventLog::open(path)) {
				DP_event_log_write_meta("Drawpile: %s", cmake_config::version());
				DP_event_log_write_meta("Qt: %s", QT_VERSION_STR);
				DP_event_log_write_meta("OS: %s", qUtf8Printable(QSysInfo::prettyProductName()));
				DP_event_log_write_meta("Input: %s", tabletinput::current());
				const desktop::settings::Settings &settings = dpApp().settings();
				DP_event_log_write_meta("Tablet enabled: %d", settings.tabletEvents());
				DP_event_log_write_meta("Tablet eraser action: %d", settings.tabletEraserAction());
				DP_event_log_write_meta("One-finger draw: %d", settings.oneFingerDraw());
				DP_event_log_write_meta("One-finger scroll: %d", settings.oneFingerScroll());
				DP_event_log_write_meta("Two-finger rotate: %d", settings.twoFingerRotate());
				DP_event_log_write_meta("Two-finger zoom: %d", settings.twoFingerZoom());
				DP_event_log_write_meta("Gestures: %d", settings.touchGestures());
			} else {
				showErrorMessageWithDetails(tr("Error opening tablet event log."), DP_error());
			}
		}
	}
}

void MainWindow::showBrushSettingsDialog()
{
	dialogs::BrushSettingsDialog *dlg = findChild<dialogs::BrushSettingsDialog *>(
		"brushsettingsdialog", Qt::FindDirectChildrenOnly);
	if(!dlg) {
		dlg = new dialogs::BrushSettingsDialog{this};
		dlg->setObjectName("brushsettingsdialog");
		dlg->setAttribute(Qt::WA_DeleteOnClose);

		tools::BrushSettings *brushSettings = m_dockToolSettings->brushSettings();
		connect(dlg, &dialogs::BrushSettingsDialog::brushSettingsChanged,
			brushSettings, &tools::BrushSettings::setCurrentBrush);
		connect(brushSettings, &tools::BrushSettings::eraseModeChanged, dlg,
			&dialogs::BrushSettingsDialog::setForceEraseMode);
		dlg->setForceEraseMode(brushSettings->isCurrentEraserSlot());

		tools::ToolController *toolCtrl = m_doc->toolCtrl();
		connect(toolCtrl, &tools::ToolController::activeBrushChanged, dlg,
			&dialogs::BrushSettingsDialog::updateUiFromActiveBrush);
		connect(toolCtrl, &tools::ToolController::stabilizerUseBrushSampleCountChanged,
			dlg, &dialogs::BrushSettingsDialog::setStabilizerUseBrushSampleCount);
		connect(toolCtrl, &tools::ToolController::globalSmoothingChanged, dlg,
			&dialogs::BrushSettingsDialog::setGlobalSmoothing);
		dlg->updateUiFromActiveBrush(toolCtrl->activeBrush());
		dlg->setStabilizerUseBrushSampleCount(toolCtrl->stabilizerUseBrushSampleCount());
		dlg->setGlobalSmoothing(toolCtrl->globalSmoothing());
	}

	utils::showWindow(dlg, shouldShowDialogMaximized());
	dlg->activateWindow();
	dlg->raise();
}

/**
 * The settings window will automatically destruct when it is closed.
 */
dialogs::SettingsDialog *MainWindow::showSettings()
{
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog(
		m_singleSession, m_smallScreenMode, getStartDialogOrThis());
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg, shouldShowDialogMaximized());
	return dlg;
}

void MainWindow::host()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Entry::Host);
}

void MainWindow::hostSession(
	const QString &title, const QString &password, const QString &alias,
	bool nsfm, const QString &announcementUrl, const QString &remoteAddress)
{
	if(!m_doc->canvas()) {
		showErrorMessage(tr("No canvas to host! Create one or open a file."));
		return;
	}

	const bool useremote = !remoteAddress.isEmpty();
	QUrl address;

	if(useremote) {
		address = net::Server::fixUpAddress(
			QUrl(
				net::Server::addSchemeToUserSuppliedAddress(remoteAddress),
				QUrl::TolerantMode),
			false);
	} else {
		address.setHost(WhatIsMyIp::guessLocalAddress());
		address.setScheme(QStringLiteral("drawpile"));
	}

	if(!address.isValid() || address.host().isEmpty()) {
		showErrorMessage(tr("Invalid address"));
		return;
	}

	// Start server if hosting locally
	const desktop::settings::Settings &settings = dpApp().settings();
	if(!useremote) {
#ifdef DP_HAVE_BUILTIN_SERVER
		canvas::PaintEngine *paintEngine = m_doc->canvas()->paintEngine();
		server::BuiltinServer *server =
			new server::BuiltinServer(paintEngine, this);

		QString errorMessage;
		bool serverStarted = server->start(
			settings.serverPort(), settings.serverTimeout(),
			settings.serverPrivateUserList(), settings.serverDnssd(),
			&errorMessage);
		if(!serverStarted) {
			QMessageBox::warning(this, tr("Host Session"), errorMessage);
			delete server;
			return;
		}

		connect(
			m_doc->client(), &net::Client::serverDisconnected, server,
			&server::BuiltinServer::stop);
		paintEngine->setServer(server);

		if(server->port() != cmake_config::proto::port()) {
			address.setPort(server->port());
		}
#else
		showErrorMessage(tr("Hosting on this computer is not available"));
		return;
#endif
	}

	// Connect to server
	net::LoginHandler *login = new net::LoginHandler(
		useremote ? net::LoginHandler::Mode::HostRemote : net::LoginHandler::Mode::HostBuiltin,
		address,
		this);
	login->setUserId(m_doc->canvas()->localUserId());
	login->setSessionAlias(alias);
	login->setPassword(password);
	login->setTitle(title);
	login->setAnnounceUrl(announcementUrl);
	login->setNsfm(nsfm);
	if(useremote) {
		utils::ScopedOverrideCursor waitCursor;
		login->setInitialState(m_doc->canvas()->generateSnapshot(
			true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS));
	}

	utils::showWindow(
		new dialogs::LoginDialog(login, getStartDialogOrThis()),
		shouldShowDialogMaximized());

	m_doc->client()->connectToServer(
		settings.serverTimeout(), login, !useremote);
}

void MainWindow::invite()
{
	dialogs::InviteDialog *dlg = new dialogs::InviteDialog(
		m_netstatus, m_doc->isCompatibilityMode(), m_doc->isSessionAllowWeb(),
		m_doc->isSessionNsfm(), this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(
		m_doc, &Document::compatibilityModeChanged, dlg,
		&dialogs::InviteDialog::setSessionCompatibilityMode);
	connect(
		m_doc, &Document::sessionAllowWebChanged, dlg,
		&dialogs::InviteDialog::setSessionAllowWeb);
	connect(
		m_doc, &Document::sessionNsfmChanged, dlg,
		&dialogs::InviteDialog::setSessionNsfm);
	dlg->show();
}

void MainWindow::join()
{
	if(m_singleSession) {
		reconnect();
	} else {
		dialogs::StartDialog *dlg = showStartDialog();
		dlg->showPage(dialogs::StartDialog::Entry::Join);
	}
}

void MainWindow::reconnect()
{
	joinSession(m_doc->client()->sessionUrl(true));
}

void MainWindow::browse()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Entry::Browse);
}

/**
 * Leave action triggered, ask for confirmation
 */
void MainWindow::leave()
{
	QMessageBox *leavebox = utils::makeQuestion(
		this,
		m_doc->sessionTitle().isEmpty() ? tr("Untitled") : m_doc->sessionTitle(),
		m_doc->client()->isBuiltin()
			? tr("Really leave and terminate the session?")
			: tr("Really leave the session?"));
	leavebox->button(QMessageBox::Yes)->setText(tr("Leave"));
	leavebox->button(QMessageBox::No)->setText(tr("Stay"));
	leavebox->setDefaultButton(QMessageBox::No);
	connect(leavebox, &QMessageBox::finished, this, [this](int result) {
		if(result == QMessageBox::Yes) {
			m_doc->client()->disconnectFromServer();
		}
	});

	if(m_doc->client()->uploadQueueBytes() > 0) {
		leavebox->setIcon(QMessageBox::Warning);
		leavebox->setInformativeText(tr("There is still unsent data! Please wait until transmission completes!"));
	}

	leavebox->show();
}

#ifndef __EMSCRIPTEN__
void MainWindow::checkForUpdates()
{
	dialogs::StartDialog *dlg = showStartDialog();
	dlg->showPage(dialogs::StartDialog::Entry::Welcome);
	dlg->checkForUpdates();
}
#endif

void MainWindow::reportAbuse()
{
	dialogs::AbuseReportDialog *dlg = new dialogs::AbuseReportDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	dlg->setSessionInfo(QString(), QString(), m_doc->sessionTitle());

	const canvas::UserListModel *userlist = m_doc->canvas()->userlist();
	for(const auto &u : userlist->users()) {
		if(u.isOnline && u.id != m_doc->canvas()->localUserId())
			dlg->addUser(u.id, u.name);
	}

	connect(dlg, &dialogs::AbuseReportDialog::accepted, this, [this, dlg]() {
		m_doc->sendAbuseReport(dlg->userId(), dlg->message());
	});

	utils::showWindow(dlg, shouldShowDialogMaximized());
}

void MainWindow::tryToGainOp()
{
	QString opword = QInputDialog::getText(
				this,
				tr("Become Operator"),
				tr("Enter operator password"),
				QLineEdit::Password
	);
	if(!opword.isEmpty())
		m_doc->sendOpword(opword);
}

void MainWindow::resetSession()
{
	utils::ScopedOverrideCursor waitCursor;
	dialogs::ResetDialog *dlg = new dialogs::ResetDialog(
		m_doc->canvas()->paintEngine(), m_doc->isCompatibilityMode(),
		m_singleSession, this);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

#ifndef SINGLE_MAIN_WINDOW
	// It's always possible to create a new document from a snapshot
	connect(dlg, &dialogs::ResetDialog::newSelected, this, [dlg]() {
		MainWindow *w = new MainWindow(false);
		w->m_doc->sendResetSession(dlg->getResetImage());
		dlg->deleteLater();
	});
#endif

	// Session resetting is available only to session operators
	if(m_doc->canvas()->aclState()->amOperator()) {
		connect(dlg, &dialogs::ResetDialog::resetSelected, this, [this, dlg]() {
			utils::ScopedOverrideCursor innerWaitCursor;
			canvas::CanvasModel *canvas = m_doc->canvas();
			if(canvas->aclState()->amOperator()) {
				net::MessageList snapshot = dlg->getResetImage();
				canvas->amendSnapshotMetadata(
					snapshot, true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);
				m_doc->sendResetSession(snapshot);
			}
			dlg->deleteLater();
		});

	} else {
		dlg->setCanReset(false);
	}

	utils::showWindow(dlg, shouldShowDialogMaximized());
}

void MainWindow::terminateSession()
{
	// When hosting on the builtin server, terminating the session isn't done
	// through mod commands, it's a matter of leaving and stopping the server.
	if(m_doc->client()->isBuiltin()) {
		leave();
	} else {
		QInputDialog *dlg = new QInputDialog(this);
		dlg->setInputMode(QInputDialog::TextInput);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setWindowTitle(tr("Terminate session"));
		dlg->setLabelText(tr("Reason:"));
		dlg->setOkButtonText(tr("Terminate"));
#ifndef __EMSCRIPTEN__
		dlg->setWindowModality(Qt::WindowModal);
#endif
		connect(
			dlg, &QInputDialog::textValueSelected, m_doc,
			&Document::sendTerminateSession);
		dlg->show();
	}
}

/**
 * @param url URL
 */
void MainWindow::joinSession(const QUrl& url, const QString &autoRecordFile)
{
	m_canvasView->hideDisconnectedWarning();
	if(!canReplace()) {
		prepareWindowReplacement();

		QStringList args;
		args.reserve(6);
		args.append(QStringLiteral("--no-restore-window-position"));
		if(m_singleSession) {
			args.append(QStringLiteral("--single-session"));
		}
		args.append(QStringLiteral("--join"));
		args.append(url.toString(QUrl::FullyEncoded));
		if(!autoRecordFile.isEmpty()) {
			args.append(QStringLiteral("--auto-record"));
			args.append(autoRecordFile);
		}

		bool newProcessStarted = dpApp().runInNewProcess(args);
		if(newProcessStarted) {
			emit windowReplacementFailed(nullptr);
		} else {
			MainWindow *win = new MainWindow(false);
			emit windowReplacementFailed(win);
			if(m_singleSession) {
				win->m_doc->client()->setSessionUrl(url);
			}
			win->joinSession(url, autoRecordFile);
		}
		return;
	}

	net::LoginHandler *login = new net::LoginHandler(
		net::LoginHandler::Mode::Join, net::Server::fixUpAddress(url, true),
		this);
	auto *dlg = new dialogs::LoginDialog(login, getStartDialogOrThis());
	connect(m_doc, &Document::catchupProgress, dlg, &dialogs::LoginDialog::catchupProgress);
	connect(m_doc, &Document::serverLoggedIn, dlg, &dialogs::LoginDialog::onLoginDone);
	connect(dlg, &dialogs::LoginDialog::destroyed, this, &MainWindow::showCompatibilityModeWarning);
	m_canvasView->connectLoginDialog(m_doc, dlg);

	dlg->show();
	m_doc->setRecordOnConnect(autoRecordFile);
	m_doc->client()->connectToServer(dpApp().settings().serverTimeout(), login, false);
}

/**
 * Now connecting to server
 */
void MainWindow::onServerConnected()
{
	// Enable connection related actions
	emit hostSessionEnabled(false);
	getAction("leavesession")->setEnabled(true);
	getAction("sessionsettings")->setEnabled(true);
	if(m_singleSession) {
		getAction("joinsession")->setEnabled(false);
	}

	// Disable UI until login completes
	m_canvasView->viewWidget()->setEnabled(false);
	setDrawingToolsEnabled(false);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::onServerDisconnected(
	const QString &message, const QString &errorcode, bool localDisconnect,
	bool anyMessageReceived)
{
	canvas::CanvasModel *canvas = m_doc->canvas();
	emit hostSessionEnabled(canvas != nullptr);
#ifdef DP_HAVE_BUILTIN_SERVER
	if(canvas) {
		canvas->paintEngine()->setServer(nullptr);
	}
#endif

	getAction("invitesession")->setEnabled(false);
	getAction("leavesession")->setEnabled(false);
	getAction("sessionsettings")->setEnabled(false);
	getAction("reportabuse")->setEnabled(false);
	getAction("terminatesession")->setEnabled(false);
	if(m_singleSession) {
		getAction("joinsession")->setEnabled(true);
	}
	m_admintools->setEnabled(false);
	m_sessionSettings->close();

	// Re-enable UI
	m_canvasView->viewWidget()->setEnabled(true);
	setDrawingToolsEnabled(true);

	// Display login error if not yet logged in
	if(!m_doc->client()->isLoggedIn() && !localDisconnect &&
	   m_lastDisconnectNotificationTimer.hasExpired()) {
		QString name = QStringLiteral("disconnectederrormessagebox");
		QMessageBox *msgbox = findChild<QMessageBox *>(name);
		if(!msgbox) {
			QString title;
			QString description;
			if(anyMessageReceived) {
				title = tr("Disconnected");
				description = tr("You've been disconnected from the server.");
			} else {
				title = tr("Connection Failed");
				description =
					tr("Could not establish a connection to the server.");
#ifdef __EMSCRIPTEN__
				browser::intuitFailedConnectionReason(
					description, m_doc->client()->connectionUrl());
#endif
			}

			msgbox =
				utils::showWarning(getStartDialogOrThis(), title, description);
			msgbox->setObjectName(name);
		}

		if(msgbox->informativeText().isEmpty() && !message.isEmpty()) {
			msgbox->setInformativeText(message);
		}

		if(errorcode == "SESSIONIDINUSE") {
			// We tried to host a session using with a vanity ID, but that
			// ID was taken. Show a button for quickly joining that session instead
			msgbox->setInformativeText(msgbox->informativeText() + "\n" + tr("Would you like to join the session instead?"));

			QAbstractButton *joinbutton = msgbox->addButton(tr("Join"), QMessageBox::YesRole);

			msgbox->removeButton(msgbox->button(QMessageBox::Ok));
			msgbox->addButton(QMessageBox::Cancel);

			QUrl url = m_doc->client()->sessionUrl(true);

			connect(joinbutton, &QAbstractButton::clicked, this, [this, url]() {
				joinSession(url, QString{});
			});

		}

		msgbox->open();
	}

	// If logged in but disconnected unexpectedly, show notification bar.
	// Always show it in single-session mode, reconnecting is the only recourse.
	if(m_singleSession || (m_doc->client()->isLoggedIn() && !localDisconnect)) {
		QString notif = message.isEmpty()
				? tr("You've been disconnected from the session.")
				: tr("Disconnected: %1").arg(message);
		m_canvasView->showDisconnectedWarning(notif, m_singleSession);
		if(m_lastDisconnectNotificationTimer.hasExpired()) {
			dpApp().notifications()->trigger(
				this, notification::Event::Disconnect, notif);
			m_lastDisconnectNotificationTimer.setRemainingTime(2000);
		}
	}

#ifndef __EMSCRIPTEN__
	if(m_exitAction != RUNNING) {
		m_exitAction = RUNNING;
		QTimer::singleShot(100, this, &QMainWindow::close);
	}
#endif
}

/**
 * Server connection established and login successfull
 */
void MainWindow::onServerLogin(bool join, const QString &joinPassword)
{
	m_initialCatchup = join;
	net::Client *client = m_doc->client();
	m_netstatus->loggedIn(client->sessionUrl(), joinPassword);
	m_netstatus->setSecurityLevel(
		client->securityLevel(), client->hostCertificate(),
		client->isSelfSignedCertificate());
	m_canvasView->viewWidget()->setEnabled(true);
	m_canvasView->hideDisconnectedWarning();
	m_sessionSettings->setPersistenceEnabled(client->serverSuppotsPersistence());
	m_sessionSettings->setBanImpExEnabled(
		client->isModerator(), client->serverSupportsCryptBanImpEx(),
		client->serverSupportsModBanImpEx());
	m_sessionSettings->setAutoResetEnabled(client->sessionSupportsAutoReset());
	m_sessionSettings->setAuthenticated(client->isAuthenticated());
	setDrawingToolsEnabled(true);
	getAction("terminatesession")->setEnabled(
		client->isModerator() || client->isBuiltin());
	onOperatorModeChange(m_doc->canvas()->aclState()->amOperator());
	getAction("reportabuse")->setEnabled(client->serverSupportsReports());
	getAction("invitesession")->setEnabled(true);
	if(m_chatbox->isCollapsed()) {
		getAction("togglechat")->trigger();
	}
	if(!join && dpApp().settings().showInviteDialogOnHost()) {
		invite();
	}
}

void MainWindow::onCompatibilityModeChanged(bool compatibilityMode)
{
	m_dockToolSettings->brushSettings()->setCompatibilityMode(compatibilityMode);
	m_dockToolSettings->transformSettings()->setCompatibilityMode(compatibilityMode);
}

void MainWindow::updateLockWidget()
{
	using Reason = view::Lock::Reason;
	QFlags<Reason> reasons = Reason::None;
	canvas::CanvasModel *canvas = m_doc->canvas();
	canvas::AclState *aclState = canvas ? canvas->aclState() : nullptr;

	if(m_doc->isSessionOutOfSpace()) {
		reasons.setFlag(Reason::OutOfSpace);
	}

	if(aclState && aclState->isResetLocked()) {
		reasons.setFlag(Reason::Reset);
	}

	bool sessionLocked = aclState && aclState->isSessionLocked();
	getAction("locksession")->setChecked(sessionLocked);
	if(sessionLocked) {
		reasons.setFlag(Reason::Canvas);
	}

	if(!m_notificationsMuted) {
		if(sessionLocked && !m_wasSessionLocked) {
			dpApp().notifications()->trigger(
				this, notification::Event::Locked, tr("Canvas locked"));
		} else if(!sessionLocked && m_wasSessionLocked) {
			dpApp().notifications()->trigger(
				this, notification::Event::Unlocked, tr("Canvas unlocked"));
		}
	}
	m_wasSessionLocked = sessionLocked;

	reasons |= m_dockLayers->currentLayerLock();

	if(aclState && aclState->isLocked(aclState->localUserId())) {
		reasons.setFlag(Reason::User);
	}

	if(m_dockToolSettings->isCurrentToolLocked()) {
		reasons.setFlag(Reason::Tool);
	}

	m_viewLock->setReasons(reasons);
	m_lockstatus->setToolTip(m_viewLock->description());
	m_lockstatus->setPixmap(reasons ? QIcon::fromTheme("object-locked").pixmap(16, 16) : QPixmap{});
}

void MainWindow::onNsfmChanged(bool nsfm)
{
	if(nsfm && parentalcontrols::level() >= parentalcontrols::Level::Restricted) {
		m_doc->client()->disconnectFromServer();
		showErrorMessage(tr("Session blocked by parental controls"));
	}
}

void MainWindow::onOperatorModeChange(bool op)
{
	m_admintools->setEnabled(op);
	m_serverLogDialog->setOperatorMode(op);
	getAction("gainop")->setEnabled(!op && m_doc->isSessionOpword());
	getAction("sessionundodepthlimit")->setEnabled(op && !m_doc->client()->isCompatibilityMode());
}

void MainWindow::onFeatureAccessChange(DP_Feature feature, bool canUse)
{
	switch(feature) {
	case DP_FEATURE_PUT_IMAGE:
		m_putimagetools->setEnabled(canUse);
		getAction("toolfill")->setEnabled(canUse);
		m_dockToolSettings->selectionSettings()->setPutImageAllowed(canUse);
		break;
	case DP_FEATURE_RESIZE:
		m_resizetools->setEnabled(canUse);
		break;
	case DP_FEATURE_BACKGROUND:
		m_canvasbgtools->setEnabled(canUse);
		break;
	case DP_FEATURE_LASER:
		getAction("toollaser")->setEnabled(canUse && getAction("showlasers")->isChecked());
		break;
	case DP_FEATURE_UNDO:
		m_undotools->setEnabled(canUse);
		break;
	case DP_FEATURE_TIMELINE:
		m_dockTimeline->setFeatureAccess(canUse);
		break;
	case DP_FEATURE_MYPAINT:
		m_dockToolSettings->brushSettings()->setMyPaintAllowed(canUse);
		break;
	default: break;
	}
	updateLockWidget();
}

void MainWindow::onUndoDepthLimitSet(int undoDepthLimit)
{
	QAction *action = getAction("sessionundodepthlimit");
	action->setProperty("undodepthlimit", undoDepthLimit);
	action->setText(tr("Undo Limit... (%1)").arg(undoDepthLimit));
	action->setStatusTip(tr("Change the session's undo limit, current limit is %1.").arg(undoDepthLimit));
}

#ifndef __EMSCRIPTEN__
/**
 * Write settings and exit. The application will not be terminated until
 * the last mainwindow is closed.
 */
void MainWindow::exit()
{
	if(windowState().testFlag(Qt::WindowFullScreen))
		toggleFullscreen();
	setDocksHidden(false);
	saveSplitterState();
	saveWindowState();
	deleteLater();
}
#endif

void MainWindow::showErrorMessage(const QString &message)
{
	showErrorMessageWithDetails(message, QString{});
}

/**
 * @param message error message
 * @param details error details
 */
void MainWindow::showErrorMessageWithDetails(const QString &message, const QString &details)
{
	if(!message.isEmpty()) {
		utils::showWarning(this, tr("Error"), message, details);
	}
}

void MainWindow::showLoadResultMessage(DP_LoadResult result)
{
	if(result != DP_LOAD_RESULT_SUCCESS) {
		QString message = impex::getLoadResultMessage(result);
		if(impex::shouldIncludeLoadResultDpError(result)) {
			showErrorMessageWithDetails(message, DP_error());
		} else {
			showErrorMessage(message);
		}
	}
}

void MainWindow::setShowAnnotations(bool show)
{
	QAction *annotationtool = getAction("tooltext");
	annotationtool->setEnabled(show);
	m_canvasView->setShowAnnotations(show);
	if(!show) {
		if(annotationtool->isChecked())
			getAction("toolbrush")->trigger();
	}
}

void MainWindow::setShowLaserTrails(bool show)
{
	QAction *lasertool = getAction("toollaser");
	lasertool->setEnabled(show);
	m_canvasView->setShowLaserTrails(show);
	if(!show) {
		if(lasertool->isChecked())
			getAction("toolbrush")->trigger();
	}
}

/**
 * @brief Enter/leave fullscreen mode
 *
 * Window position and configuration is saved when entering fullscreen mode
 * and restored when leaving. On Android, the window is always fullscreen.
 */
void MainWindow::toggleFullscreen()
{
#ifndef SINGLE_MAIN_WINDOW
	if(windowState().testFlag(Qt::WindowFullScreen)==false) {
		// Save windowed mode state
		m_fullscreenOldGeometry = geometry();
		m_fullscreenOldMaximized = isMaximized();
		showFullScreen();
	} else {
		// Restore old state
		if(m_fullscreenOldMaximized) {
			showMaximized();
		} else {
			showNormal();
			setGeometry(m_fullscreenOldGeometry);
		}
	}
#endif
}

void MainWindow::setFreezeDocks(bool freeze)
{
	const auto features = QDockWidget::DockWidgetClosable
		| QDockWidget::DockWidgetMovable
		| QDockWidget::DockWidgetFloatable;

	for(auto *dw : findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
		if(freeze)
			dw->setFeatures(dw->features() & ~features);
		else
			dw->setFeatures(dw->features() | features);
	}

	for(QToolBar *tb : findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly)) {
		tb->setMovable(!freeze);
	}
}

void MainWindow::setDocksHidden(bool hidden)
{
	keepCanvasPosition([this, hidden] {
		m_viewStatusBar->setHidden(hidden);
		if(hidden) {
			m_hiddenDockState = saveState();
			for(QWidget *w : findChildren<QWidget *>(
					QString(), Qt::FindDirectChildrenOnly)) {
				bool shouldHide =
					w->isVisible() &&
					(w->inherits("QDockWidget") || w->inherits("QToolBar"));
				if(shouldHide) {
					w->hide();
				}
			}
			// Force recalculation of the central widget's position. Otherwise
			// this will happen lazily on the next repaint and we can't scroll
			// properly. Doing it this way is clearly a hack, but I can't figure
			// out another way to do it that doesn't introduce flicker or the
			// window resizing.
			restoreState(saveState());
		} else {
			restoreState(m_hiddenDockState);
			m_hiddenDockState.clear();
		}
	});
	m_dockToggles->setDisabled(hidden);
}

void MainWindow::setDockTitleBarsHidden(bool hidden)
{
	QAction *freezeDocks = getAction("freezedocks");
	QAction *hideDockTitleBars = getAction("hidedocktitlebars");
	bool actuallyHidden = hidden && !freezeDocks->isChecked()
		&& hideDockTitleBars->isChecked() && !m_smallScreenMode;
	if(actuallyHidden != m_titleBarsHidden) {
		m_titleBarsHidden = hidden;
		for(auto *dw : findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
				dw->titleBarWidget()->setHidden(hidden);
		}
	}
}

void MainWindow::updateSideTabDocks()
{
	if(m_smallScreenMode || getAction("sidetabdocks")->isChecked()) {
		setDockOptions(dockOptions() | QMainWindow::VerticalTabs);
	} else {
		setDockOptions(dockOptions() & ~QMainWindow::VerticalTabs);
	}
}

void MainWindow::handleToggleAction(int action)
{
	using Action = drawingboard::ToggleItem::Action;
	utils::ScopedUpdateDisabler disabler{this};
	QScopedValueRollback<bool> rollback(m_updatingInterfaceMode, true);
	keepCanvasPosition([this, action] {
		if(!m_dockToggles->isEnabled()) {
			getAction("hidedocks")->toggle();
		}

		m_viewStatusBar->hide();

		QPair<QWidget *, int> dockActions[] = {
			{m_dockToolSettings, int(Action::Left)},
			{m_dockBrushPalette, int(Action::Left)},
			{m_dockTimeline, int(Action::Top)},
			{m_dockOnionSkins, int(Action::Top)},
			{m_dockNavigator, int(Action::None)},
			{m_dockColorSpinner, int(Action::Right)},
			{m_dockColorSliders, int(Action::Right)},
			{m_dockColorPalette, int(Action::Right)},
			{m_dockLayers, int(Action::Right)},
			{m_chatbox, int(Action::Bottom)},
		};
		QVector<QWidget *> docksToShow;

		for(const QPair<QWidget *, int> &p : dockActions) {
			QWidget *dock = p.first;
			bool isActivated =
				action != int(Action::None) && action == p.second;
			bool isVisible = dock->isVisible();
			dock->hide();
			bool visible = isActivated ? !isVisible : false;
			if(visible) {
				docksToShow.append(dock);
			}
		}

		for(QWidget *dock : docksToShow) {
			dock->show();
		}
		m_viewStatusBar->setVisible(docksToShow.isEmpty());

		bool chatVisible = m_chatbox->isVisible();
		QAction *togglechat = getAction("togglechat");
		QSignalBlocker blocker{togglechat};
		togglechat->setChecked(chatVisible);
		if(chatVisible) {
			int h = height();
			int top = h / 2;
			m_splitter->setSizes({top, h - top});
		}

		// It's ridiculous how hard resizeDocks() resists resizing the docks to
		// the size it's told to. Doing it several times seems to do the trick.
		for(int i = 0; i < 5; ++i) {
			QCoreApplication::processEvents();
			setDefaultDockSizes();
		}

#ifdef SINGLE_MAIN_WINDOW
		resize(compat::widgetScreen(*this)->availableSize());
#endif
	});
}

void MainWindow::setNotificationsMuted(bool muted)
{
	m_notificationsMuted = muted;
}

void MainWindow::setBusy(bool busy)
{
	setDrawingToolsEnabled(!busy);
}

/**
 * User selected a tool
 * @param tool action representing the tool
 */
void MainWindow::selectTool(QAction *tool)
{
	// Note. Actions must be in the same order in the enum and the group
	int idx = m_drawingtools->actions().indexOf(tool);
	Q_ASSERT(idx>=0);
	if(idx<0)
		return;

	if(m_dockToolSettings->currentTool() == idx) {
		if(dpApp().settings().toolToggle())
			m_dockToolSettings->setPreviousTool();
		m_tempToolSwitchShortcut->reset();
	} else {
		m_dockToolSettings->setTool(tools::Tool::Type(idx));
		m_toolChangeTime.start();
	}
}

/**
 * @brief Handle tool change
 * @param tool
 */
void MainWindow::toolChanged(tools::Tool::Type tool)
{
	QAction *toolaction = m_drawingtools->actions().at(int(tool));
	toolaction->setChecked(true);

	// When using the annotation tool, highlight all text boxes
	m_canvasView->setShowAnnotationBorders(tool==tools::Tool::ANNOTATION);

	// Show own user marker if laser pointer is selected.
	bool isLaserPointerSelected = tool == tools::Tool::LASERPOINTER;
	m_canvasView->setShowOwnUserMarker(isLaserPointerSelected);

	// Send pointer updates when using the laser pointer
	m_canvasView->setPointerTracking(
		isLaserPointerSelected &&
		m_dockToolSettings->laserPointerSettings()->pointerTracking());

	// Deselect annotation when tool changed
	if(tool != tools::Tool::ANNOTATION)
		m_doc->toolCtrl()->setActiveAnnotation(0);

	m_doc->toolCtrl()->setActiveTool(tool);
	updateLockWidget();
}

void MainWindow::updateFreehandToolButton(int brushMode)
{
	QString iconName;
	QString toolTip;
	QString statusTip;
	switch(brushMode) {
	case tools::BrushSettings::EraseMode:
		iconName = QStringLiteral("drawpile_brusherase");
		toolTip = tr("Freehand (erase mode, click to reset)");
		statusTip = tr("Freehand brush tool (erase mode)");
		break;
	case tools::BrushSettings::AlphaLockMode:
		iconName = QStringLiteral("drawpile_brushlock");
		toolTip = tr("Freehand (alpha lock mode, click to reset)");
		statusTip = tr("Freehand brush tool (alpha lock mode)");
		break;
	case tools::BrushSettings::NormalMode:
		iconName = QStringLiteral("draw-brush");
		toolTip = m_freehandAction->toolTip();
		statusTip = m_freehandAction->statusTip();
		break;
	default:
		return; // Eraser slot active, don't mess with the icon.
	}
	m_freehandButton->setIcon(QIcon::fromTheme(iconName));
	m_freehandButton->setToolTip(toolTip);
	m_freehandButton->setStatusTip(statusTip);
}

void MainWindow::handleFreehandToolButtonClicked()
{
	QSignalBlocker blocker(m_freehandButton);
	if(m_dockToolSettings->currentTool() == tools::Tool::FREEHAND) {
		switch(m_dockToolSettings->brushSettings()->getBrushMode()) {
		case tools::BrushSettings::EraseMode:
		case tools::BrushSettings::AlphaLockMode:
			m_dockToolSettings->brushSettings()->resetBrushMode();
			m_freehandButton->setChecked(m_freehandAction->isChecked());
			return;
		default:
			break;
		}
	}
	m_freehandAction->trigger();
}

void MainWindow::updateSelectTransformActions()
{
	canvas::CanvasModel *canvas = m_doc->canvas();
	bool haveTransform = canvas && canvas->transform()->isActive();
	bool canApplyTransform =
		haveTransform && canvas->transform()->isDstQuadValid();
	bool haveSelection =
		!haveTransform && canvas && canvas->selection()->isValid();
	bool haveAnnotation =
		getAction("tooltext")->isChecked() &&
		m_dockToolSettings->annotationSettings()->selected() != 0;

#ifdef __EMSCRIPTEN__
	getAction("downloadselection")->setEnabled(haveSelection);
#else
	getAction("saveselection")->setEnabled(haveSelection);
#endif

	getAction("cutlayer")->setEnabled(haveSelection);
	getAction("copylayer")->setEnabled(haveSelection);
	getAction("copyvisible")->setEnabled(haveSelection);
	getAction("copymerged")->setEnabled(haveSelection);
	getAction("paste")->setEnabled(!haveTransform);
	getAction("paste-centered")->setEnabled(!haveTransform);
	getAction("pastefile")->setEnabled(!haveTransform);
	getAction("stamp")->setEnabled(canApplyTransform);
	getAction("cleararea")->setEnabled(haveSelection || haveAnnotation);
	getAction("selectall")->setEnabled(!haveTransform);
	getAction("selectnone")->setEnabled(haveSelection);
	getAction("selectinvert")->setEnabled(haveSelection);
	getAction("selectlayerbounds")->setEnabled(!haveTransform);
	getAction("selectlayercontents")->setEnabled(!haveTransform);
	getAction("fillfgarea")->setEnabled(haveSelection);
	getAction("recolorarea")->setEnabled(haveSelection);
	getAction("colorerasearea")->setEnabled(haveSelection);
	getAction("starttransform")->setEnabled(haveSelection);
	getAction("transformmirror")->setEnabled(haveTransform);
	getAction("transformflip")->setEnabled(haveTransform);
	getAction("transformrotatecw")->setEnabled(haveTransform);
	getAction("transformrotateccw")->setEnabled(haveTransform);
}

void MainWindow::copyText()
{
	// Attempt to copy text if a text widget has focus
	QWidget *focus = QApplication::focusWidget();

	auto *textedit = qobject_cast<QTextEdit*>(focus);
	if(textedit)
		textedit->copy();
}

void MainWindow::paste()
{
	const QMimeData *mimeData = Document::getClipboardData();
	if(mimeData && mimeData->hasImage()) {
		QPoint pastepos;
		bool pasteAtPos = false;

		// Get source position
		QByteArray srcpos = mimeData->data("x-drawpile/pastesrc");
		if(!srcpos.isNull()) {
			QList<QByteArray> pos = srcpos.split(',');
			if(pos.size() == 4) {
				bool ok1, ok2, ok3, ok4;
				pastepos = QPoint(pos.at(0).toInt(&ok1), pos.at(1).toInt(&ok2));
				qint64 pid = pos.at(2).toLongLong(&ok3);
				qulonglong doc = pos.at(3).toULongLong(&ok4);
				pasteAtPos = ok1 && ok2 && ok3 && ok4 &&
					pid == qApp->applicationPid() && doc == m_doc->pasteId();
			}
		}

		// Paste-in-place if we're the source (same process, same document)
		if(pasteAtPos && m_canvasView->isPointVisible(pastepos))
			pasteImage(mimeData->imageData().value<QImage>(), &pastepos, true);
		else
			pasteImage(mimeData->imageData().value<QImage>());
	}
}

void MainWindow::pasteCentered()
{
	const QMimeData *mimeData = Document::getClipboardData();
	if(mimeData && mimeData->hasImage()) {
		pasteImage(mimeData->imageData().value<QImage>(), nullptr, true);
	}
}

void MainWindow::pasteFile()
{
	FileWrangler::ImageOpenFn imageOpenCompleted = [this](QImage &img) {
		if(img.isNull()) {
			showErrorMessage(tr("The image could not be loaded"));
		} else {
			pasteImage(img);
		}
	};
	FileWrangler(this).openPasteImage(imageOpenCompleted);
}

void MainWindow::pasteFilePath(const QString &path)
{
	QImage img(path);
	if(img.isNull()) {
		showErrorMessage(tr("The image could not be loaded"));
	} else {
		pasteImage(img);
	}
}

void MainWindow::pasteImage(
	const QImage &image, const QPoint *point, bool force)
{
	canvas::CanvasModel *canvas = m_canvasView->canvas();
	if(canvas && canvas->aclState()->canUseFeature(DP_FEATURE_PUT_IMAGE) &&
	   !canvas->transform()->isActive() && !image.isNull() &&
	   !image.size().isEmpty()) {
		QRect srcBounds = canvas->getPasteBounds(
			image.size(), point ? *point : m_canvasView->viewCenterPoint(),
			force);
		if(!srcBounds.isEmpty()) {
			m_dockToolSettings->startTransformPaste(
				srcBounds,
				image.convertToFormat(QImage::Format_ARGB32_Premultiplied));
		}
	}
}

void MainWindow::dropImage(const QImage &image)
{
	pasteImage(image);
}

void MainWindow::dropUrl(const QUrl &url)
{
	if(url.isLocalFile()) {
		QString path = url.toLocalFile();
		if(m_canvasView->canvas()) {
			pasteFilePath(path);
		} else {
			openPath(path);
		}
	}
}

void MainWindow::clearOrDelete()
{
	// This slot is triggered in response to the 'Clear' action, which
	// which in turn can be triggered via the 'Delete' shortcut. In annotation
	// editing mode, the current selection may be an annotation, so we should delete
	// that instead of clearing out the canvas.
	QAction *annotationtool = getAction("tooltext");
	if(annotationtool->isChecked()) {
		const uint16_t a = m_dockToolSettings->annotationSettings()->selected();
		if(a>0) {
			net::Client *client = m_doc->client();
			uint8_t contextId = client->myId();
			net::Message messages[] = {
				net::makeUndoPointMessage(contextId),
				net::makeAnnotationDeleteMessage(contextId, a),
			};
			client->sendMessages(DP_ARRAY_LENGTH(messages), messages);
			return;
		}
	}

	// No annotation selected: clear seleted area as usual
	m_doc->clearArea();
}

void MainWindow::resizeCanvas()
{
	canvas::CanvasModel *canvas = m_doc->canvas();
	if(!canvas) {
		qWarning("resizeCanvas: no canvas!");
		return;
	}

	const QSize size = m_doc->canvas()->size();
	dialogs::ResizeDialog *dlg = new dialogs::ResizeDialog(size, this);
	canvas::PaintEngine *paintEngine = m_doc->canvas()->paintEngine();
	dlg->setBackgroundColor(paintEngine->historyBackgroundColor());
	dlg->setPreviewImage(paintEngine->renderPixmap().scaled(
		300, 300, Qt::KeepAspectRatio));
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	// Preset crop from selection if one exists
	if (canvas->selection()->isValid()) {
		dlg->setBounds(canvas->selection()->bounds());
	}

	connect(dlg, &QDialog::accepted, this, [this, dlg]() {
		if (m_doc->canvas()->selection()) {
			// m_doc->canvas()->setSelection(nullptr);
		}
		dialogs::ResizeVector r = dlg->resizeVector();
		if(!r.isZero()) {
			m_doc->sendResizeCanvas(r.top, r.right, r.bottom, r.left);
		}
	});
	utils::showWindow(dlg, shouldShowDialogMaximized());
}

static QIcon makeBackgroundColorIcon(QColor &color)
{
	static constexpr int SIZE = 16;
	static constexpr int HALF = SIZE / 2;
	QPixmap pixmap{SIZE, SIZE};
	pixmap.fill(color);
	if(color.alpha() != 255) {
		QPainter painter{&pixmap};
		painter.setCompositionMode(QPainter::CompositionMode_DestinationOver);
		painter.fillRect(0, 0, HALF, HALF, Qt::gray);
		painter.fillRect(HALF, 0, HALF, HALF, Qt::white);
		painter.fillRect(0, HALF, HALF, HALF, Qt::white);
		painter.fillRect(HALF, HALF, HALF, HALF, Qt::gray);
	}
	return pixmap;
}

void MainWindow::updateBackgroundActions()
{
	QAction *canvasBackground = getAction("canvas-background");
	QAction *setLocalBackground = getAction("set-local-background");
	QAction *clearLocalBackground = getAction("clear-local-background");
	canvas::CanvasModel *canvas = m_doc->canvas();
	if(canvas) {
		canvasBackground->setEnabled(true);
		setLocalBackground->setEnabled(true);

		canvas::PaintEngine *paintEngine = canvas->paintEngine();
		QColor sessionColor = paintEngine->historyBackgroundColor();
		canvasBackground->setIcon(makeBackgroundColorIcon(sessionColor));

		QColor localColor;
		if(paintEngine->localBackgroundColor(localColor)) {
			setLocalBackground->setIcon(makeBackgroundColorIcon(localColor));
			clearLocalBackground->setEnabled(true);
		} else {
			setLocalBackground->setIcon(QIcon{});
			clearLocalBackground->setEnabled(false);
		}
	} else {
		QIcon nullIcon = QIcon{};
		canvasBackground->setIcon(nullIcon);
		canvasBackground->setEnabled(false);
		setLocalBackground->setIcon(nullIcon);
		setLocalBackground->setEnabled(false);
		clearLocalBackground->setEnabled(false);
	}
}

void MainWindow::changeCanvasBackground()
{
	if(!m_doc->canvas()) {
		qWarning("changeCanvasBackground: no canvas!");
		return;
	}
	color_widgets::ColorDialog *dlg = dialogs::newDeleteOnCloseColorDialog(
		m_doc->canvas()->paintEngine()->historyBackgroundColor(), this);
	connect(dlg, &color_widgets::ColorDialog::colorSelected, m_doc, &Document::sendCanvasBackground);
	utils::showWindow(dlg, shouldShowDialogMaximized());
}

void MainWindow::changeLocalCanvasBackground()
{
	if(!m_doc->canvas()) {
		qWarning("changeLocalCanvasBackground: no canvas!");
		return;
	}

	canvas::PaintEngine *paintEngine = m_doc->canvas()->paintEngine();
	QColor color;
	if(!paintEngine->localBackgroundColor(color)) {
		color = paintEngine->historyBackgroundColor();
	}

	color_widgets::ColorDialog *dlg =
		dialogs::newDeleteOnCloseColorDialog(color, this);
	connect(
		dlg, &color_widgets::ColorDialog::colorSelected, paintEngine,
		&canvas::PaintEngine::setLocalBackgroundColor);
	utils::showWindow(dlg, shouldShowDialogMaximized());
}

void MainWindow::clearLocalCanvasBackground()
{
	if(!m_doc->canvas()) {
		qWarning("clearLocalCanvasBackground: no canvas!");
		return;
	}
	m_doc->canvas()->paintEngine()->clearLocalBackgroundColor();
}


void MainWindow::showLayoutsDialog()
{
	dialogs::LayoutsDialog *dlg = findChild<dialogs::LayoutsDialog *>(
		"layoutsdialog", Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->setParent(getStartDialogOrThis());
	} else {
		dlg = new dialogs::LayoutsDialog{saveState(), getStartDialogOrThis()};
		dlg->setObjectName("layoutsdialog");
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		connect(dlg, &dialogs::LayoutsDialog::applyState, [this](const QByteArray &state) {
			restoreState(state);
		});
	}
	dlg->show();
	dlg->activateWindow();
	dlg->raise();
}

void MainWindow::showUserInfoDialog(int userId)
{
	for(auto *dlg : findChildren<dialogs::UserInfoDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
		if(dlg->userId() == userId) {
			dlg->triggerUpdate();
			dlg->activateWindow();
			dlg->raise();
			return;
		}
	}

	canvas::User user = m_doc->canvas()->userlist()->getOptionalUserById(userId)
		.value_or(canvas::User{
			userId, tr("User #%1").arg(userId), {}, false, false, false, false,
			false, false, false, false, false});
	dialogs::UserInfoDialog *dlg = new dialogs::UserInfoDialog{user, this};
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::UserInfoDialog::requestUserInfo, this,
		&MainWindow::requestUserInfo);
	connect(m_doc->client(), &net::Client::userInfoReceived, dlg,
		&dialogs::UserInfoDialog::receiveUserInfo);
	dlg->triggerUpdate();
	dlg->show();
}


void MainWindow::changeUndoDepthLimit()
{
	QAction *action = getAction("sessionundodepthlimit");
	bool ok;
	int previousUndoDepthLimit = action->property("undodepthlimit").toInt(&ok);

	dialogs::SessionUndoDepthLimitDialog dlg{
		ok ? previousUndoDepthLimit : DP_DUMP_UNDO_DEPTH_LIMIT, this};
	if(dlg.exec() == QDialog::Accepted) {
		int undoDepthLimit = dlg.undoDepthLimit();
		if(undoDepthLimit != previousUndoDepthLimit) {
			m_doc->client()->sendMessage(net::makeUndoDepthMessage(
				m_doc->canvas()->localUserId(), undoDepthLimit));
		}
	}
}


void MainWindow::updateDevToolsActions()
{
	QAction *tabletEventLogAction = getAction("tableteventlog");
	tabletEventLogAction->setText(drawdance::EventLog::isOpen() ? tr("Stop Tablet Event Log") : tr("Tablet Event Log..."));

	QAction *profileAction = getAction("profile");
	profileAction->setText(drawdance::Perf::isOpen() ? tr("Stop Profile") : tr("Profile..."));

	QAction *artificialLagAction = getAction("artificiallag");
	net::Client *client = m_doc->client();
	artificialLagAction->setEnabled(client->isConnected());
	int artificialLagMs = client->artificialLagMs();
	artificialLagAction->setText(
		tr("Set Artificial Lag... (currently %1 ms)").arg(artificialLagMs));

	QAction *artificialDisconnectAction = getAction("artificialdisconnect");
	artificialDisconnectAction->setEnabled(client->isConnected());

	QAction *debugDumpAction = getAction("debugdump");
	debugDumpAction->setChecked(m_doc->wantCanvasHistoryDump());
}

void MainWindow::setArtificialLag()
{
	bool ok;
	int artificalLagMs = QInputDialog::getInt(
		this, tr("Set Artificial Lag"),
		tr("Artificial lag in milliseconds (0 to disable):"),
		m_doc->client()->artificialLagMs(), 0, INT_MAX, 1, &ok);
	if(ok) {
		m_doc->client()->setArtificialLagMs(artificalLagMs);
	}
}

void MainWindow::setArtificialDisconnect()
{
	bool ok;
	int seconds = QInputDialog::getInt(
		this, tr("Artificial Disconnect"),
		tr("Simulate a disconnect after this many seconds:"),
		1, 0, INT_MAX, 1, &ok);
	if(ok) {
		QTimer::singleShot(seconds * 1000, m_doc->client(),
			&net::Client::artificialDisconnect);
	}
}

void MainWindow::toggleDebugDump()
{
	if(m_doc->wantCanvasHistoryDump()) {
		m_doc->setWantCanvasHistoryDump(false);
	} else {
		QString path = utils::paths::writablePath("dumps");
		QMessageBox::StandardButton result = QMessageBox::question(
			this, tr("Record Debug Dumps"),
			tr("Debug dumps will record local and remote drawing commands. "
				"They can be used to fix network issues, but not much else. "
				"If you want to make a regular recording, use File > Record... "
				"instead.\n\nDebug dump recording starts on the next canvas "
				"reset and the files will be saved in %1\n\nAre you sure you"
				"want to start recording debug dumps?").arg(path));
		if(result == QMessageBox::Yes) {
			m_doc->setWantCanvasHistoryDump(true);
		}
	}
}

void MainWindow::openDebugDump()
{
	FileWrangler(this).openDebugDump(
		std::bind(&MainWindow::openPath, this, _1, _2));
}


void MainWindow::about()
{
	auto [pixelSize, mmSize] = DrawpileApp::screenResolution();
	QMessageBox::about(nullptr, tr("About Drawpile"),
			QStringLiteral("<p><b>Drawpile %1</b> (%2)<br>").arg(cmake_config::version(), QSysInfo::buildCpuArchitecture()) +
			tr("A collaborative drawing program.") + QStringLiteral("</p>"

			"<p>Copyright © askmeaboutloom and contributors. Originally made by Calle Laakkonen.</p>"

			"<p>This program is free software; you may redistribute it and/or "
			"modify it under the terms of the GNU General Public License as "
			"published by the Free Software Foundation, either version 3, or "
			"(at your opinion) any later version.</p>"

			"<p>This program is distributed in the hope that it will be useful, "
			"but WITHOUT ANY WARRANTY; without even the implied warranty of "
			"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the "
			"GNU General Public License for more details.</p>"

			"<p>You should have received a copy of the GNU General Public License "
			"along with this program.  If not, see <a href=\"http://www.gnu.org/licences/\">http://www.gnu.org/licenses/</a>.</p>"
			) +
			QStringLiteral("<hr><p><b>%1</b> %2</p><p><b>%3</b> %4</p><p><b>%5</b> %6</p>")
				.arg(tr("Settings File:"))
				.arg(dpApp().settings().path().toHtmlEscaped())
				.arg(tr("Tablet Input:"))
				.arg(QCoreApplication::translate("tabletinput", tabletinput::current()))
				.arg(tr("Primary screen:"))
				.arg(tr("%1x%2px² (%3x%4mm²)")
					.arg(pixelSize.width()).arg(pixelSize.height())
					.arg(mmSize.width()).arg(mmSize.height())));
}

void MainWindow::homepage()
{
	QDesktopServices::openUrl(QUrl(cmake_config::website()));
}

/**
 * @brief Create a new action.
 *
 * All created actions are added to a list that is used in the
 * settings dialog to edit the shortcuts.
 *
 * @param name (internal) name of the action.
 * @param text action text
 */
ActionBuilder MainWindow::makeAction(const char *name, const QString& text)
{
	Q_ASSERT(name);
	QAction *act = new QAction(text, this);
	act->setObjectName(name);
	act->setAutoRepeat(false);

	// Add this action to the mainwindow so its shortcut can be used
	// even when the menu/toolbar is not visible
	addAction(act);

	return ActionBuilder(act);
}

QAction *MainWindow::getAction(const QString &name)
{
	QAction *a = findChild<QAction*>(name, Qt::FindDirectChildrenOnly);
	if(!a)
		qFatal("%s: no such action", qPrintable(name));
	Q_ASSERT(a);
	return a;
}

/**
 * @brief Create actions, menus and toolbars
 */
void MainWindow::setupActions()
{
	Q_ASSERT(m_doc);
	Q_ASSERT(m_dockLayers);

	// Action groups
	m_currentdoctools = new QActionGroup(this);
	m_currentdoctools->setExclusive(false);
	m_currentdoctools->setEnabled(false);

	m_admintools = new QActionGroup(this);
	m_admintools->setExclusive(false);

	m_canvasbgtools = new QActionGroup(this);
	m_canvasbgtools->setEnabled(false);

	m_resizetools = new QActionGroup(this);
	m_resizetools->setEnabled(false);

	m_putimagetools = new QActionGroup(this);
	m_putimagetools->setEnabled(false);

	m_undotools = new QActionGroup(this);
	m_undotools->setEnabled(false);

	m_drawingtools = new QActionGroup(this);
	connect(m_drawingtools, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	QMenu *toggletoolbarmenu = new QMenu(this);
	QMenu *toggledockmenu = new QMenu(this);
	m_dockToggles = new QActionGroup{this};
	m_dockToggles->setExclusive(false);

	m_desktopModeActions = new QActionGroup(this);
	m_desktopModeActions->setExclusive(false);
	m_smallScreenModeActions = new QActionGroup(this);
	m_smallScreenModeActions->setExclusive(false);

	// Collect list of docks for dock menu
	for(const auto *dw : findChildren<const QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		QAction *toggledockaction = dw->toggleViewAction();
		Q_ASSERT(!dw->objectName().isEmpty());
		Q_ASSERT(toggledockaction->objectName().isEmpty());
		toggledockaction->setObjectName(
			QStringLiteral("toggledock%1").arg(dw->objectName()));
		toggledockmenu->addAction(toggledockaction);
		m_dockToggles->addAction(toggledockaction);
		CustomShortcutModel::registerCustomizableAction(
			toggledockaction->objectName(),
			tr("Toggle Dock %1").arg(toggledockaction->text()),
			toggledockaction->shortcut(), QKeySequence());
		addAction(toggledockaction);
		m_desktopModeActions->addAction(toggledockaction);
	}

	toggledockmenu->addSeparator();
	QAction *freezeDocks = makeAction("freezedocks", tr("Lock Docks")).noDefaultShortcut().checkable().remembered();
	toggledockmenu->addAction(freezeDocks);
	m_desktopModeActions->addAction(freezeDocks);
	connect(freezeDocks, &QAction::toggled, this, &MainWindow::setFreezeDocks);

	QAction *sideTabDocks = makeAction("sidetabdocks", tr("Vertical Tabs on Sides")).noDefaultShortcut().checkable().remembered();
	toggledockmenu->addAction(sideTabDocks);
	m_desktopModeActions->addAction(sideTabDocks);
	connect(
		sideTabDocks, &QAction::toggled, this, &MainWindow::updateSideTabDocks);
	updateSideTabDocks();

	QAction *hideDocks = makeAction("hidedocks", tr("Hide Docks")).checkable().shortcut("tab");
	toggledockmenu->addAction(hideDocks);
	m_desktopModeActions->addAction(hideDocks);
	connect(hideDocks, &QAction::toggled, this, &MainWindow::setDocksHidden);

	QAction *hideDockTitleBars = makeAction("hidedocktitlebars", tr("Hold Shift to Arrange")).noDefaultShortcut().checkable().remembered();
	toggledockmenu->addAction(hideDockTitleBars);
	m_desktopModeActions->addAction(hideDockTitleBars);
	connect(hideDocks, &QAction::toggled, [this](){
		setDockTitleBarsHidden(m_titleBarsHidden);
	});

	//
	// File menu and toolbar
	//
	QAction *newdocument = makeAction("newdocument", tr("&New")).icon("document-new").shortcut(QKeySequence::New);
	QAction *open = makeAction("opendocument", tr("&Open...")).icon("document-open").shortcut(QKeySequence::Open);
#ifdef Q_OS_MACOS
	QAction *closefile = makeAction("closedocument", tr("Close")).shortcut(QKeySequence::Close);
#endif
#ifdef __EMSCRIPTEN__
	QAction *download = makeAction("downloaddocument", tr("&Download Image…")).icon("document-save").shortcut(QKeySequence::Save);
	QAction *downloadsel = makeAction("downloadselection", tr("Download Selection…")).icon("select-rectangular").noDefaultShortcut();
#else
	QAction *save = makeAction("savedocument", tr("&Save")).icon("document-save").shortcut(QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", tr("Save &As...")).icon("document-save-as").shortcut(QKeySequence::SaveAs);
	QAction *exportDocument = makeAction("exportdocument", tr("Export Image…")).icon("document-export").noDefaultShortcut();
	QAction *savesel = makeAction("saveselection", tr("Export Selection...")).icon("select-rectangular").noDefaultShortcut();
	QAction *autosave = makeAction("autosave", tr("Autosave")).noDefaultShortcut().checkable().disabled();
	QAction *exportTemplate = makeAction("exporttemplate", tr("Export Session &Template...")).noDefaultShortcut();
	QAction *exportGifAnimation = makeAction("exportanimgif", tr("Export Animated &GIF...")).noDefaultShortcut();
#ifndef Q_OS_ANDROID
	QAction *exportAnimationFrames = makeAction("exportanimframes", tr("Export Animation &Frames...")).noDefaultShortcut();
#endif
#endif
	QAction *importOldAnimation = makeAction("importoldanimation", tr("Import &Drawpile 2.1 Animation…")).noDefaultShortcut();
	QAction *importBrushes = makeAction("importbrushes", tr("Import &Brushes...")).noDefaultShortcut();
	QAction *exportBrushes = makeAction("exportbrushes", tr("Export &Brushes…")).noDefaultShortcut();

#ifndef __EMSCRIPTEN__
	QAction *record = makeAction("recordsession", tr("Record...")).icon("media-record").noDefaultShortcut();
#endif
	QAction *start = makeAction("start", tr("Start...")).noDefaultShortcut();
#ifndef __EMSCRIPTEN__
	QAction *quit = makeAction("exitprogram", tr("&Quit")).icon("application-exit").shortcut("Ctrl+Q").menuRole(QAction::QuitRole);
#endif

#ifdef Q_OS_MACOS
	m_currentdoctools->addAction(closefile);
#endif
#ifdef __EMSCRIPTEN__
	m_currentdoctools->addAction(download);
	m_currentdoctools->addAction(downloadsel);
#else
	m_currentdoctools->addAction(save);
	m_currentdoctools->addAction(saveas);
	m_currentdoctools->addAction(exportTemplate);
	m_currentdoctools->addAction(savesel);
	m_currentdoctools->addAction(exportDocument);
	m_currentdoctools->addAction(exportGifAnimation);
#ifndef Q_OS_ANDROID
	m_currentdoctools->addAction(exportAnimationFrames);
#endif
	m_currentdoctools->addAction(record);
#endif

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
#ifdef __EMSCRIPTEN__
	connect(download, &QAction::triggered, this, &MainWindow::download);
	connect(downloadsel, &QAction::triggered, this, &MainWindow::downloadSelection);
#else
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));
	connect(exportDocument, &QAction::triggered, this, &MainWindow::exportImage);
	connect(exportTemplate, &QAction::triggered, this, &MainWindow::exportTemplate);
	connect(savesel, &QAction::triggered, this, &MainWindow::saveSelection);

	connect(autosave, &QAction::triggered, m_doc, &Document::setAutosave);
	connect(m_doc, &Document::autosaveChanged, autosave, &QAction::setChecked);
	connect(m_doc, &Document::canAutosaveChanged, autosave, &QAction::setEnabled);

	connect(exportGifAnimation, &QAction::triggered, this, &MainWindow::exportGifAnimation);
#ifndef Q_OS_ANDROID
	connect(exportAnimationFrames, &QAction::triggered, this, &MainWindow::exportAnimationFrames);
#endif
	connect(record, &QAction::triggered, this, &MainWindow::toggleRecording);
#endif
	connect(importOldAnimation, &QAction::triggered, this, &MainWindow::importOldAnimation);
	connect(importBrushes, &QAction::triggered, m_dockBrushPalette, &docks::BrushPalette::importBrushes);
	connect(exportBrushes, &QAction::triggered, m_dockBrushPalette, &docks::BrushPalette::exportBrushes);
	connect(start, &QAction::triggered, this, &MainWindow::start);

#ifndef __EMSCRIPTEN__
#	ifdef Q_OS_MACOS
	connect(closefile, SIGNAL(triggered()), this, SLOT(close()));
	connect(quit, SIGNAL(triggered()), MacMenu::instance(), SLOT(quitAll()));
#	else
	connect(quit, SIGNAL(triggered()), this, SLOT(close()));
#	endif
#endif

	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(newdocument);
	filemenu->addAction(open);
#ifndef __EMSCRIPTEN__
	if(!m_singleSession) {
		m_recentMenu = filemenu->addMenu(tr("Open &Recent"));
		m_recentMenu->setIcon(QIcon::fromTheme("document-open-recent"));
	}
#endif
	filemenu->addSeparator();

#ifdef __EMSCRIPTEN__
	filemenu->addAction(download);
	filemenu->addAction(downloadsel);
#else
#	ifdef Q_OS_MACOS
	filemenu->addAction(closefile);
#	endif
	filemenu->addAction(save);
	filemenu->addAction(saveas);
	filemenu->addAction(savesel);
	filemenu->addAction(exportDocument);
	filemenu->addAction(autosave);
#endif
	filemenu->addSeparator();

	QMenu *importMenu = filemenu->addMenu(tr("&Import"));
	importMenu->setIcon(QIcon::fromTheme("document-import"));
	importMenu->addAction(importOldAnimation);
	importMenu->addAction(importBrushes);

	QMenu *exportMenu = filemenu->addMenu(tr("&Export"));
	exportMenu->setIcon(QIcon::fromTheme("document-export"));
#ifndef __EMSCRIPTEN__
	exportMenu->addAction(exportDocument);
	exportMenu->addAction(exportTemplate);
	exportMenu->addAction(exportGifAnimation);
#	ifndef Q_OS_ANDROID
	exportMenu->addAction(exportAnimationFrames);
#	endif
#endif
	exportMenu->addAction(exportBrushes);
#ifndef __EMSCRIPTEN__
	filemenu->addAction(record);
#endif
	filemenu->addSeparator();
	filemenu->addAction(start);
#ifndef __EMSCRIPTEN__
	filemenu->addAction(quit);
#endif

	m_toolBarFile = new QToolBar(tr("File Tools"));
	m_toolBarFile->setObjectName("filetoolsbar");
	toggletoolbarmenu->addAction(m_toolBarFile->toggleViewAction());
	if(!m_singleSession) {
		m_toolBarFile->addAction(newdocument);
		m_toolBarFile->addAction(open);
	}
#ifdef __EMSCRIPTEN__
	m_toolBarFile->addAction(download);
#else
	m_toolBarFile->addAction(save);
	m_toolBarFile->addAction(record);
#endif

#ifndef __EMSCRIPTEN__
	if(!m_singleSession) {
		connect(m_recentMenu, &QMenu::triggered, this, [this](QAction *action) {
			QVariant filepath = action->property("filepath");
			if(filepath.isValid()) {
				this->openPath(filepath.toString());
			} else {
				dialogs::StartDialog *dlg = showStartDialog();
				dlg->showPage(dialogs::StartDialog::Entry::Recent);
			}
		});
	}
#endif

	//
	// Edit menu
	//
#ifdef Q_OS_ANDROID
	QKeySequence undoShortcut = QKeySequence{Qt::Key_VolumeUp};
	QKeySequence redoShortcut = QKeySequence{Qt::Key_VolumeDown};
	QKeySequence undoAlternateShortcut = QKeySequence::Undo;
	QKeySequence redoAlternateShortcut = QKeySequence::Redo;
#else
	QKeySequence undoShortcut = QKeySequence::Undo;
	QKeySequence undoAlternateShortcut = QKeySequence();
#if defined(Q_OS_WIN) || defined(__EMSCRIPTEN__)
	QKeySequence redoShortcut = QKeySequence("Ctrl+Y");
	QKeySequence redoAlternateShortcut = QKeySequence("Ctrl+Shift+Z");
#elif defined(Q_OS_LINUX)
	QKeySequence redoShortcut = QKeySequence("Ctrl+Shift+Z");
	QKeySequence redoAlternateShortcut = QKeySequence("Ctrl+Y");
#else
	QKeySequence redoShortcut = QKeySequence::Redo;
	QKeySequence redoAlternateShortcut = QKeySequence();
#endif
#endif
	QAction *undo = makeAction("undo", tr("&Undo")).icon("edit-undo").shortcut(undoShortcut, undoAlternateShortcut).autoRepeat();
	QAction *redo = makeAction("redo", tr("&Redo")).icon("edit-redo").shortcut(redoShortcut, redoAlternateShortcut).autoRepeat();
	QAction *copy = makeAction("copyvisible", tr("&Copy Merged")).icon("edit-copy").statusTip(tr("Copy selected area to the clipboard")).shortcut("Shift+Ctrl+C");
	QAction *copyMerged = makeAction("copymerged", tr("Copy Without Background")).icon("edit-copy").statusTip(tr("Copy selected area, excluding the background, to the clipboard")).shortcut("Ctrl+Alt+C");
	QAction *copylayer = makeAction("copylayer", tr("Copy From &Layer")).icon("edit-copy").statusTip(tr("Copy selected area of the current layer to the clipboard")).shortcut(QKeySequence::Copy);
	QAction *cutlayer = makeAction("cutlayer", tr("Cu&t From Layer")).icon("edit-cut").statusTip(tr("Cut selected area of the current layer to the clipboard")).shortcut(QKeySequence::Cut);
	QAction *paste = makeAction("paste", tr("&Paste")).icon("edit-paste").shortcut(QKeySequence::Paste);
	QAction *pasteCentered = makeAction("paste-centered", tr("Paste in View Center")).icon("edit-paste").shortcut("Ctrl+Shift+V");
#ifndef SINGLE_MAIN_WINDOW
	QAction *pickFromScreen = makeAction("pickfromscreen", tr("Pic&k From Screen")).icon("monitor").shortcut("Shift+I");
#endif

	QAction *pastefile = makeAction("pastefile", tr("Paste &From File...")).icon("document-open").noDefaultShortcut();
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", tr("Delete Empty Annotations")).noDefaultShortcut();
	QAction *resize = makeAction("resizecanvas", tr("Resi&ze Canvas...")).noDefaultShortcut();
	QAction *canvasBackground = makeAction("canvas-background", tr("Set Session Background...")).noDefaultShortcut();
	QAction *setLocalBackground = makeAction("set-local-background", tr("Set Local Background...")).noDefaultShortcut();
	QAction *clearLocalBackground = makeAction("clear-local-background", tr("Clear Local Background")).noDefaultShortcut();
	QAction *brushSettings = makeAction("brushsettings", tr("&Brush Settings")).icon("draw-brush").shortcut("F7");
	QAction *preferences = makeAction("preferences", tr("Prefere&nces")).icon("configure").noDefaultShortcut().menuRole(QAction::PreferencesRole);

#ifdef Q_OS_WIN32
	QVector<QAction *> drivers;
	drivers.append(makeAction("driverkistabletwindowsink", QCoreApplication::translate("dialogs::settingsdialog::Input", "KisTablet Windows Ink")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::KisTabletWinink)));
	drivers.append(makeAction("driverkistabletwintab", QCoreApplication::translate("dialogs::settingsdialog::Input", "KisTablet Wintab")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::KisTabletWintab)));
	drivers.append(makeAction("driverkistabletwintabrelative", QCoreApplication::translate("dialogs::settingsdialog::Input", "KisTablet Wintab Relative")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::KisTabletWintabRelativePenHack)));
#	if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	drivers.append(makeAction("driverqt5", QCoreApplication::translate("dialogs::settingsdialog::Input", "Qt5")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::Qt5)));
#	else
	drivers.append(makeAction("driverqt6windowsink", QCoreApplication::translate("dialogs::settingsdialog::Input", "Qt6 Windows Ink")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::Qt6Winink)));
	drivers.append(makeAction("driverqt6wintab", QCoreApplication::translate("dialogs::settingsdialog::Input", "Qt6 Wintab")).noDefaultShortcut().checkable().property("tabletdriver", int(tabletinput::Mode::Qt6Wintab)));
#	endif
#endif

	QAction *expandup = makeAction("expandup", tr("Expand &Up")).shortcut(CTRL_KEY | Qt::Key_J);
	QAction *expanddown = makeAction("expanddown", tr("Expand &Down")).shortcut(CTRL_KEY | Qt::Key_K);
	QAction *expandleft = makeAction("expandleft", tr("Expand &Left")).shortcut(CTRL_KEY | Qt::Key_H);
	QAction *expandright = makeAction("expandright", tr("Expand &Right")).shortcut(CTRL_KEY | Qt::Key_L);

	QAction *cleararea = makeAction("cleararea", tr("Delete"))
							 .shortcut(QKeySequence::Delete)
							 .icon("trash-empty");

	m_currentdoctools->addAction(copy);
	m_currentdoctools->addAction(copylayer);
	m_currentdoctools->addAction(deleteAnnotations);

	m_undotools->addAction(undo);
	m_undotools->addAction(redo);

	m_putimagetools->addAction(cutlayer);
	m_putimagetools->addAction(paste);
	m_putimagetools->addAction(pasteCentered);
	m_putimagetools->addAction(pastefile);
	m_putimagetools->addAction(cleararea);

	m_canvasbgtools->addAction(canvasBackground);
	m_resizetools->addAction(resize);
	m_resizetools->addAction(expandup);
	m_resizetools->addAction(expanddown);
	m_resizetools->addAction(expandleft);
	m_resizetools->addAction(expandright);

	connect(undo, &QAction::triggered, m_doc, &Document::undo);
	connect(redo, &QAction::triggered, m_doc, &Document::redo);
	connect(copy, &QAction::triggered, m_doc, &Document::copyVisible);
	connect(copyMerged, &QAction::triggered, m_doc, &Document::copyMerged);
	connect(copylayer, &QAction::triggered, m_doc, &Document::copyLayer);
	connect(cutlayer, &QAction::triggered, m_doc, &Document::cutLayer);
	connect(paste, &QAction::triggered, this, &MainWindow::paste);
	connect(pasteCentered, &QAction::triggered, this, &MainWindow::pasteCentered);
#ifndef SINGLE_MAIN_WINDOW
	connect(
		pickFromScreen, &QAction::triggered,
		m_dockToolSettings->colorPickerSettings(),
		&tools::ColorPickerSettings::startPickFromScreen);
#endif
	connect(pastefile, SIGNAL(triggered()), this, SLOT(pasteFile()));
	connect(deleteAnnotations, &QAction::triggered, m_doc, &Document::removeEmptyAnnotations);
	connect(cleararea, &QAction::triggered, this, &MainWindow::clearOrDelete);
	connect(resize, SIGNAL(triggered()), this, SLOT(resizeCanvas()));
	connect(canvasBackground, &QAction::triggered, this, &MainWindow::changeCanvasBackground);
	connect(setLocalBackground, &QAction::triggered, this, &MainWindow::changeLocalCanvasBackground);
	connect(clearLocalBackground, &QAction::triggered, this, &MainWindow::clearLocalCanvasBackground);
	connect(brushSettings, &QAction::triggered, this, &MainWindow::showBrushSettingsDialog);
	connect(preferences, SIGNAL(triggered()), this, SLOT(showSettings()));
#ifdef Q_OS_WIN32
	for(QAction *driver : drivers) {
		connect(driver, &QAction::triggered, this, [this, driver](bool checked) {
			if(checked) {
				dpApp().settings().setTabletDriver(tabletinput::Mode(driver->property("tabletdriver").toInt()));
			}
		});
	}
#endif

	// Expanding by multiples of tile size allows efficient resizing
	connect(expandup, &QAction::triggered, this, [this] { m_doc->sendResizeCanvas(64, 0 ,0, 0);});
	connect(expandright, &QAction::triggered, this, [this] { m_doc->sendResizeCanvas(0, 64, 0, 0);});
	connect(expanddown, &QAction::triggered, this, [this] { m_doc->sendResizeCanvas(0,0, 64, 0);});
	connect(expandleft, &QAction::triggered, this, [this] { m_doc->sendResizeCanvas(0,0, 0, 64);});

	QMenu *editmenu = menuBar()->addMenu(tr("&Edit"));
	editmenu->addAction(undo);
	editmenu->addAction(redo);
	editmenu->addSeparator();
	editmenu->addAction(cutlayer);
	editmenu->addAction(copy);
	editmenu->addAction(copyMerged);
	editmenu->addAction(copylayer);
	editmenu->addAction(paste);
	editmenu->addAction(pasteCentered);
	editmenu->addAction(pastefile);
#ifndef SINGLE_MAIN_WINDOW
	editmenu->addAction(pickFromScreen);
#endif
	editmenu->addSeparator();

	editmenu->addAction(resize);
	QMenu *expandmenu = editmenu->addMenu(tr("&Expand Canvas"));
	expandmenu->addAction(expandup);
	expandmenu->addAction(expanddown);
	expandmenu->addAction(expandleft);
	expandmenu->addAction(expandright);
	QMenu *backgroundmenu = editmenu->addMenu(tr("Canvas Background"));
	backgroundmenu->addAction(canvasBackground);
	backgroundmenu->addAction(setLocalBackground);
	backgroundmenu->addAction(clearLocalBackground);
	connect(backgroundmenu, &QMenu::aboutToShow, this, &MainWindow::updateBackgroundActions);

	editmenu->addSeparator();
	editmenu->addAction(deleteAnnotations);
	editmenu->addAction(cleararea);
	editmenu->addSeparator();
	editmenu->addAction(brushSettings);
#ifdef Q_OS_WIN32
	QMenu *driverMenu = editmenu->addMenu(QIcon::fromTheme("dialog-input-devices"), tr("Tablet Driver"));
	for(QAction *driver : drivers) {
		driverMenu->addAction(driver);
	}
	connect(driverMenu, &QMenu::aboutToShow, this, [this, drivers]() {
		tabletinput::Mode mode = dpApp().settings().tabletDriver();
		for(QAction *driver : drivers) {
			QSignalBlocker blocker(driver);
			driver->setChecked(driver->property("tabletdriver").toInt() == int(mode));
		}
	});
#endif
	editmenu->addAction(preferences);

	m_toolBarEdit = new QToolBar(tr("Edit Tools"));
	m_toolBarEdit->setObjectName("edittoolsbar");
	toggletoolbarmenu->addAction(m_toolBarEdit->toggleViewAction());
	m_toolBarEdit->addAction(undo);
	m_toolBarEdit->addAction(redo);
	m_toolBarEdit->addAction(cutlayer);
	m_toolBarEdit->addAction(copylayer);
	m_toolBarEdit->addAction(paste);
	m_toolBarEdit->addWidget(m_dualColorButton);

	//
	// View menu
	//
	QAction *layoutsAction = makeAction("layouts", tr("&Layouts...")).icon("window_").shortcut("F9");

	QAction *toolbartoggles = new QAction(tr("&Toolbars"), this);
	toolbartoggles->setMenu(toggletoolbarmenu);

	QAction *docktoggles = new QAction(tr("&Docks"), this);
	docktoggles->setMenu(toggledockmenu);

	QAction *toggleChat = makeAction("togglechat", tr("Chat")).shortcut("Alt+C").checked();

	QAction *moveleft = makeAction("moveleft", tr("Move Canvas Left")).noDefaultShortcut().autoRepeat();
	QAction *moveright = makeAction("moveright", tr("Move Canvas Right")).noDefaultShortcut().autoRepeat();
	QAction *moveup = makeAction("moveup", tr("Move Canvas Up")).noDefaultShortcut().autoRepeat();
	QAction *movedown = makeAction("movedown", tr("Move Canvas Down")).noDefaultShortcut().autoRepeat();
	QAction *zoomin = makeAction("zoomin", tr("Zoom &In")).icon("zoom-in").shortcut(QKeySequence::ZoomIn).autoRepeat();
	QAction *zoomout = makeAction("zoomout", tr("Zoom &Out")).icon("zoom-out").shortcut(QKeySequence::ZoomOut).autoRepeat();
	QAction *zoomorig = makeAction("zoomone", tr("&Normal Size")).icon("zoom-original").shortcut(QKeySequence("ctrl+0"));
	QAction *zoomfit = makeAction("zoomfit", tr("&Fit Page")).icon("zoom-select").noDefaultShortcut();
	QAction *zoomfitwidth = makeAction("zoomfitwidth", tr("Fit Page &Width")).icon("zoom-fit-width").noDefaultShortcut();
	QAction *zoomfitheight = makeAction("zoomfitheight", tr("Fit Page &Height")).icon("zoom-fit-height").noDefaultShortcut();
	QAction *rotateorig = makeAction("rotatezero", tr("&Reset Rotation")).icon("transform-rotate").shortcut(QKeySequence("ctrl+r"));
	QAction *rotatecw = makeAction("rotatecw", tr("Rotate Canvas Clockwise")).shortcut(QKeySequence("shift+.")).icon("object-rotate-right").autoRepeat();
	QAction *rotateccw = makeAction("rotateccw", tr("Rotate Canvas Counterclockwise")).shortcut(QKeySequence("shift+,")).icon("object-rotate-left").autoRepeat();

	QAction *viewmirror = makeAction("viewmirror", tr("Mirror")).icon("object-flip-horizontal").shortcut("V").checkable();
	QAction *viewflip = makeAction("viewflip", tr("Flip")).icon("object-flip-vertical").shortcut("C").checkable();

	QAction *showannotations = makeAction("showannotations", tr("Show &Annotations")).noDefaultShortcut().checked().remembered();
	QAction *showusermarkers = makeAction("showusermarkers", tr("Show User &Pointers")).noDefaultShortcut().checked().remembered();
	QAction *showusernames = makeAction("showmarkernames", tr("Show Names")).noDefaultShortcut().checked().remembered();
	QAction *showuserlayers = makeAction("showmarkerlayers", tr("Show Layers")).noDefaultShortcut().checked().remembered();
	QAction *showuseravatars = makeAction("showmarkeravatars", tr("Show Avatars")).noDefaultShortcut().checked().remembered();
	QAction *evadeusercursors = makeAction("evadeusercursors", tr("Hide From Cursor")).noDefaultShortcut().checked().remembered();
	QAction *showlasers = makeAction("showlasers", tr("Show La&ser Trails")).noDefaultShortcut().checked().remembered();
	QAction *showgrid = makeAction("showgrid", tr("Show Pixel &Grid")).noDefaultShortcut().checked().remembered();
	QAction *showrulers = makeAction("showrulers", tr("Show &Rulers")).noDefaultShortcut().checkable().remembered();

#ifndef SINGLE_MAIN_WINDOW
	QAction *fullscreen = makeAction("fullscreen", tr("&Full Screen")).shortcut(QKeySequence::FullScreen).checkable();
	if(windowHandle()) { // mainwindow should always be a native window, but better safe than sorry
		connect(windowHandle(), &QWindow::windowStateChanged, fullscreen, [fullscreen](Qt::WindowState state) {
			// Update the mode tickmark on fulscreen state change.
			// On Qt 5.3.0, this signal doesn't seem to get emitted on OSX when clicking
			// on the toggle button in the titlebar. The state can be queried correctly though.
			fullscreen->setChecked(state & Qt::WindowFullScreen);
		});
	}
#endif

	connect(layoutsAction, &QAction::triggered, this, &MainWindow::showLayoutsDialog);

	connect(m_statusChatButton, &QToolButton::clicked, toggleChat, &QAction::trigger);

	connect(m_chatbox, &widgets::ChatBox::requestUserInfo, this, &MainWindow::showUserInfoDialog);
	connect(m_chatbox, &widgets::ChatBox::requestCurrentBrush, this, &MainWindow::requestCurrentBrush);
	connect(m_chatbox, &widgets::ChatBox::expandedChanged, toggleChat, &QAction::setChecked);
	connect(m_chatbox, &widgets::ChatBox::expandedChanged, m_statusChatButton, &QToolButton::hide);
	connect(m_chatbox, &widgets::ChatBox::expandPlease, toggleChat, &QAction::trigger);
	connect(toggleChat, &QAction::triggered, this, [this](bool show) {
		if(m_smallScreenMode) {
			handleToggleAction(int(drawingboard::ToggleItem::Action::Bottom));
		} else {
			QList<int> sizes;
			if(show) {
				QVariant oldHeight = m_chatbox->property("oldheight");
				if(oldHeight.isNull()) {
					const int h = height();
					sizes << h * 2 / 3;
					sizes << h / 3;
				} else {
					const int oh = oldHeight.toInt();
					sizes << height() - oh;
					sizes << oh;
				}
				m_chatbox->focusInput();
			} else {
				m_chatbox->setProperty("oldheight", m_chatbox->height());
				sizes << 1;
				sizes << 0;
			}
			m_splitter->setSizes(sizes);
			m_saveSplitterDebounce.start();
		}
	});
	connect(m_chatbox, &widgets::ChatBox::muteChanged, this, &MainWindow::setNotificationsMuted);

	connect(
		showrulers, &QAction::toggled, m_canvasFrame,
		&widgets::CanvasFrame::setShowRulers);

	m_canvasView->connectActions(
		{moveleft, moveright, moveup, movedown, zoomin, zoomout, zoomorig,
		 zoomfit, zoomfitwidth, zoomfitheight, rotateorig, rotatecw, rotateccw,
		 viewflip, viewmirror, showgrid, showusermarkers, showusernames,
		 showuserlayers, showuseravatars, evadeusercursors});

#ifndef SINGLE_MAIN_WINDOW
	connect(fullscreen, &QAction::triggered, this, &MainWindow::toggleFullscreen);
#endif

	connect(showannotations, &QAction::toggled, this, &MainWindow::setShowAnnotations);
	connect(showlasers, &QAction::toggled, this, &MainWindow::setShowLaserTrails);

	m_viewstatus->setActions(viewflip, viewmirror, rotateorig, {zoomorig, zoomfit, zoomfitwidth, zoomfitheight});

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(layoutsAction);
	m_desktopModeActions->addAction(layoutsAction);
	viewmenu->addAction(toolbartoggles);
	m_desktopModeActions->addAction(toolbartoggles);
	viewmenu->addAction(docktoggles);
	m_desktopModeActions->addAction(docktoggles);
	viewmenu->addAction(toggleChat);
	m_desktopModeActions->addAction(toggleChat);
	m_desktopModeActions->addAction(viewmenu->addSeparator());

	QMenu *zoommenu = viewmenu->addMenu(tr("&Zoom"));
	zoommenu->addAction(zoomin);
	zoommenu->addAction(zoomout);
	zoommenu->addAction(zoomorig);
	zoommenu->addAction(zoomfit);
	zoommenu->addAction(zoomfitwidth);
	zoommenu->addAction(zoomfitheight);

	QMenu *rotatemenu = viewmenu->addMenu(tr("Rotation"));
	rotatemenu->addAction(rotateorig);
	rotatemenu->addAction(rotatecw);
	rotatemenu->addAction(rotateccw);

	viewmenu->addAction(viewflip);
	viewmenu->addAction(viewmirror);

	viewmenu->addSeparator();

	QAction *layerViewNormal = makeAction("layerviewnormal", tr("Normal View")).statusTip(tr("Show all layers normally")).noDefaultShortcut().checkable().checked();
	QAction *layerViewCurrentLayer = makeAction("layerviewcurrentlayer", tr("Layer View")).statusTip(tr("Show only the current layer")).shortcut("Home").checkable();
	QAction *layerViewCurrentFrame = makeAction("layerviewcurrentframe", tr("Frame View")).statusTip(tr("Show only layers in the current frame")).shortcut("Shift+Home").checkable();
	QAction *layerUncensor = makeAction("layerviewuncensor", tr("Show Censored Layers")).noDefaultShortcut().checkable();
	m_lastLayerViewMode = layerViewNormal;

	QActionGroup *layerViewModeGroup = new QActionGroup(this);
	layerViewModeGroup->setExclusive(true);
	layerViewModeGroup->addAction(layerViewNormal);
	layerViewModeGroup->addAction(layerViewCurrentLayer);
	layerViewModeGroup->addAction(layerViewCurrentFrame);

	QMenu *layerViewMenu = viewmenu->addMenu(tr("Layer View Mode"));
	layerViewMenu->addAction(layerViewNormal);
	layerViewMenu->addAction(layerViewCurrentLayer);
	layerViewMenu->addAction(layerViewCurrentFrame);
	viewmenu->addAction(layerUncensor);

	connect(layerViewModeGroup, &QActionGroup::triggered, this, &MainWindow::toggleLayerViewMode);
	connect(layerUncensor, &QAction::toggled, this, &MainWindow::updateLayerViewMode);

	viewmenu->addSeparator();
	QMenu *userpointermenu = viewmenu->addMenu(tr("User Pointers"));
	userpointermenu->addAction(showusermarkers);
	userpointermenu->addAction(showlasers);
	userpointermenu->addSeparator();
	userpointermenu->addAction(showusernames);
	userpointermenu->addAction(showuserlayers);
	userpointermenu->addAction(showuseravatars);
	userpointermenu->addAction(evadeusercursors);

	QMenu *stayTimeMenu = userpointermenu->addMenu(tr("Stay Time"));
	QActionGroup *stayTimeGroup = new QActionGroup(this);
	stayTimeGroup->setExclusive(true);
	QPair<QString, int> stayTimeActions[] = {
		{tr("1 Second", "user pointer stay time"), 1000},
		{tr("10 Seconds", "user pointer stay time"), 10000},
		{tr("1 Minute", "user pointer stay time"), 60000},
		{tr("1 Hour", "user pointer stay time"), 3600000},
		{tr("Forever", "user pointer stay time"), -1},
	};
	int userMarkerPersistence = dpApp().settings().userMarkerPersistence();
	for(const QPair<QString, int> &p : stayTimeActions) {
		QAction *action = stayTimeMenu->addAction(p.first);
		action->setCheckable(true);
		int persistence = p.second;
		action->setChecked(userMarkerPersistence == persistence);
		stayTimeGroup->addAction(action);
		connect(
			action, &QAction::toggled, this, [persistence](bool checked) {
				if(checked) {
					dpApp().settings().setUserMarkerPersistence(persistence);
				}
			});
	}

	viewmenu->addAction(showannotations);

	viewmenu->addAction(showgrid);
	viewmenu->addAction(showrulers);

#ifndef SINGLE_MAIN_WINDOW
	viewmenu->addSeparator();
	viewmenu->addAction(fullscreen);
#endif

	//
	// Layer menu
	//
	QAction *layerAdd = makeAction("layeradd", tr("New Layer")).shortcut("Shift+Ctrl+Insert").icon("list-add");
	QAction *groupAdd = makeAction("groupadd", tr("New Layer Group")).icon("folder-new").noDefaultShortcut();
	QAction *layerDupe = makeAction("layerdupe", tr("Duplicate Layer")).icon("edit-copy").noDefaultShortcut();
	QAction *layerMerge = makeAction("layermerge", tr("Merge Layer")).icon("arrow-down-double").noDefaultShortcut();
	QAction *layerProperties = makeAction("layerproperties", tr("Layer Properties…")).icon("configure").noDefaultShortcut();
	QAction *layerDelete = makeAction("layerdelete", tr("Delete Layer")).icon("trash-empty").noDefaultShortcut();
	QAction *layerSetFillSource = makeAction("layersetfillsource", tr("Set as Fill Source")).icon("fill-color").noDefaultShortcut();

	QAction *layerUpAct = makeAction("layer-up", tr("Select Above")).shortcut("Shift+X").autoRepeat();
	QAction *layerDownAct = makeAction("layer-down", tr("Select Below")).shortcut("Shift+Z").autoRepeat();

	connect(layerUpAct, &QAction::triggered, m_dockLayers, &docks::LayerList::selectAbove);
	connect(layerDownAct, &QAction::triggered, m_dockLayers, &docks::LayerList::selectBelow);

	QMenu *layerMenu = menuBar()->addMenu(tr("&Layer"));
	layerMenu->addAction(layerAdd);
	layerMenu->addAction(groupAdd);
	layerMenu->addAction(layerDupe);
	layerMenu->addAction(layerMerge);
	layerMenu->addAction(layerDelete);
	layerMenu->addAction(layerProperties);
	layerMenu->addAction(layerSetFillSource);

	layerMenu->addSeparator();
	layerMenu->addAction(layerUpAct);
	layerMenu->addAction(layerDownAct);

	//
	// Select menu
	//
	// Deselect is not defined on macOS and Windows.
#if defined(Q_OS_MACOS) || defined(Q_OS_WIN)
#	define DESELECT_SHORTCUT "Ctrl+Shift+A"
#else
#	define DESELECT_SHORTCUT QKeySequence::Deselect
#endif

	QAction *selectall = makeAction("selectall", tr("Select &All"))
							 .shortcut(QKeySequence::SelectAll)
							 .icon("edit-select-all");
	QAction *selectnone = makeAction("selectnone", tr("&Deselect"))
							  .shortcut(DESELECT_SHORTCUT)
							  .icon("edit-select-none");
	QAction *selectinvert = makeAction("selectinvert", tr("&Invert Selection"))
								.noDefaultShortcut()
								.icon("edit-select-invert");
	QAction *selectlayerbounds =
		makeAction("selectlayerbounds", tr("Select Layer &Bounds"))
			.shortcut(Qt::SHIFT | Qt::Key_B)
			.icon("select-rectangular");
	QAction *selectlayercontents =
		makeAction("selectlayercontents", tr("&Layer to Selection"))
			.shortcut(Qt::SHIFT | Qt::Key_L)
			.icon("edit-image");
	QAction *fillfgarea = makeAction("fillfgarea", tr("Fill Selection"))
							  .shortcut(CTRL_KEY | Qt::Key_Comma);
	QAction *recolorarea = makeAction("recolorarea", tr("Recolor Selection"))
							   .shortcut(CTRL_KEY | Qt::SHIFT | Qt::Key_Comma);
	QAction *colorerasearea =
		makeAction("colorerasearea", tr("Color Erase Selection"))
			.shortcut(Qt::SHIFT | Qt::Key_Delete);
	QAction *starttransform =
		makeAction("starttransform", tr("&Transform"))
			.shortcut("T")
			.icon("transform-move")
			.statusTip(
				tr("Transform the selection, switch back tools afterwards"));
	QAction *transformmirror =
		makeAction("transformmirror", tr("&Mirror Transform"))
			.icon("object-flip-horizontal")
			.noDefaultShortcut();
	QAction *transformflip = makeAction("transformflip", tr("&Flip Transform"))
								 .icon("object-flip-vertical")
								 .noDefaultShortcut();
	QAction *transformrotatecw =
		makeAction("transformrotatecw", tr("&Rotate Transform Clockwise"))
			.icon("object-rotate-right")
			.noDefaultShortcut();
	QAction *transformrotateccw =
		makeAction(
			"transformrotateccw", tr("Rotate Transform &Counter-Clockwise"))
			.icon("object-rotate-left")
			.noDefaultShortcut();
	QAction *transformshrinktoview =
		makeAction(
			"transformshrinktoview", tr("Shrink Transform to &Fit View"))
			.icon("zoom-out")
			.noDefaultShortcut();
	QAction *stamp =
		makeAction("stamp", tr("&Stamp Transform")).shortcut("Ctrl+T");

	m_currentdoctools->addAction(selectall);
	m_currentdoctools->addAction(selectnone);
	m_currentdoctools->addAction(selectinvert);
	m_currentdoctools->addAction(selectlayerbounds);
	m_currentdoctools->addAction(selectlayercontents);

	m_putimagetools->addAction(fillfgarea);
	m_putimagetools->addAction(recolorarea);
	m_putimagetools->addAction(colorerasearea);
	m_putimagetools->addAction(stamp);

	connect(selectall, &QAction::triggered, m_doc, &Document::selectAll);
	connect(selectnone, &QAction::triggered, m_doc, &Document::selectNone);
	connect(selectinvert, &QAction::triggered, m_doc, &Document::selectInvert);
	connect(
		selectlayerbounds, &QAction::triggered, m_doc,
		&Document::selectLayerBounds);
	connect(
		selectlayercontents, &QAction::triggered, m_doc,
		&Document::selectLayerContents);
	connect(
		fillfgarea, &QAction::triggered, this,
		std::bind(
			&MainWindow::fillAreaWithBlendMode, this, DP_BLEND_MODE_NORMAL));
	connect(
		recolorarea, &QAction::triggered, this,
		std::bind(
			&MainWindow::fillAreaWithBlendMode, this, DP_BLEND_MODE_RECOLOR));
	connect(
		colorerasearea, &QAction::triggered, this,
		std::bind(
			&MainWindow::fillAreaWithBlendMode, this,
			DP_BLEND_MODE_COLOR_ERASE));
	connect(
		starttransform, &QAction::triggered, m_dockToolSettings,
		&docks::ToolSettings::startTransformMove);

	QMenu *selectMenu = menuBar()->addMenu(tr("Sele&ct"));
	selectMenu->addAction(selectall);
	selectMenu->addAction(selectnone);
	selectMenu->addAction(selectinvert);
	selectMenu->addAction(selectlayerbounds);
	selectMenu->addAction(selectlayercontents);
	selectMenu->addSeparator();
	selectMenu->addAction(cleararea);
	selectMenu->addAction(fillfgarea);
	selectMenu->addAction(recolorarea);
	selectMenu->addAction(colorerasearea);
	selectMenu->addSeparator();
	selectMenu->addAction(starttransform);
	selectMenu->addAction(transformmirror);
	selectMenu->addAction(transformflip);
	selectMenu->addAction(transformrotatecw);
	selectMenu->addAction(transformrotateccw);
	selectMenu->addAction(transformshrinktoview);
	selectMenu->addAction(stamp);

	m_dockToolSettings->selectionSettings()->setAction(starttransform);
	m_dockToolSettings->transformSettings()->setActions(
		transformmirror, transformflip, transformrotatecw, transformrotateccw,
		transformshrinktoview, stamp);

	//
	// Animation menu
	//
	QAction *showFlipbook = makeAction("showflipbook", tr("Flipbook")).icon("media-playback-start").statusTip(tr("Show animation preview window")).shortcut("Ctrl+F");
	QAction *frameCountSet = makeAction("frame-count-set", tr("Change Frame Count...")).icon("edit-rename").noDefaultShortcut();
	QAction *framerateSet = makeAction("framerate-set", tr("Change Frame Rate (FPS)...")).icon("edit-rename").noDefaultShortcut();
	QAction *keyFrameSetLayer = makeAction("key-frame-set-layer", tr("Set Key Frame to Current Layer")).icon("keyframe").shortcut("Ctrl+Shift+F");
	QAction *keyFrameSetEmpty = makeAction("key-frame-set-empty", tr("Set Blank Key Frame")).icon("keyframe-disable").shortcut("Ctrl+Shift+B");
	QAction *keyFrameCut = makeAction("key-frame-cut", tr("Cut Key Frame")).icon("edit-cut").noDefaultShortcut();
	QAction *keyFrameCopy = makeAction("key-frame-copy", tr("Copy Key Frame")).icon("edit-copy").noDefaultShortcut();
	QAction *keyFramePaste = makeAction("key-frame-paste", tr("Paste Key Frame")).icon("edit-paste").noDefaultShortcut();
	QAction *keyFrameProperties = makeAction("key-frame-properties", tr("Key Frame Properties...")).icon("configure").shortcut("Ctrl+Shift+P");
	QAction *keyFrameDelete = makeAction("key-frame-delete", tr("Delete Key Frame")).icon("keyframe-remove").shortcut("Ctrl+Shift+G");
	QAction *keyFrameExposureIncrease = makeAction("key-frame-exposure-increase", tr("Increase Key Frame Exposure")).icon("sidebar-expand-left").shortcut("Ctrl+Shift++");
	QAction *keyFrameExposureDecrease = makeAction("key-frame-exposure-decrease", tr("Decrease Key Frame Exposure")).icon("sidebar-collapse-left").shortcut("Ctrl+Shift+-");
	QAction *trackAdd = makeAction("track-add", tr("New Track")).icon("list-add").noDefaultShortcut();
	QAction *trackVisible = makeAction("track-visible", tr("Track Visible")).checkable().noDefaultShortcut();
	QAction *trackOnionSkin = makeAction("track-onion-skin", tr("Track Onion Skin")).checkable().shortcut("Ctrl+Shift+O");
	QAction *trackDuplicate = makeAction("track-duplicate", tr("Duplicate Track")).icon("edit-copy").noDefaultShortcut();
	QAction *trackRetitle = makeAction("track-retitle", tr("Rename Track")).icon("edit-rename").noDefaultShortcut();
	QAction *trackDelete = makeAction("track-delete", tr("Delete Track")).icon("trash-empty").noDefaultShortcut();
	QAction *frameNext = makeAction("frame-next", tr("Next Frame")).icon("keyframe-next").shortcut("Ctrl+Shift+L").autoRepeat();
	QAction *framePrev = makeAction("frame-prev", tr("Previous Frame")).icon("keyframe-previous").shortcut("Ctrl+Shift+H").autoRepeat();
	QAction *keyFrameNext = makeAction("key-frame-next", tr("Next Key Frame")).icon("keyframe-next").shortcut("Ctrl+Alt+Shift+L").autoRepeat();
	QAction *keyFramePrev = makeAction("key-frame-prev", tr("Previous Key Frame")).icon("keyframe-previous").shortcut("Ctrl+Alt+Shift+H").autoRepeat();
	QAction *trackAbove = makeAction("track-above", tr("Track Above")).icon("arrow-up").shortcut("Ctrl+Shift+K").autoRepeat();
	QAction *trackBelow = makeAction("track-below", tr("Track Below")).icon("arrow-down").shortcut("Ctrl+Shift+J").autoRepeat();

	QAction *keyFrameCreateLayer = makeAction("key-frame-create-layer", tr("Create Layer on Current Key Frame")).icon("keyframe-add").shortcut("Ctrl+Shift+R");
	QAction *keyFrameCreateLayerNext = makeAction("key-frame-create-layer-next", tr("Create Layer on Next Key Frame")).icon("keyframe-next").shortcut("Ctrl+Shift+T");
	QAction *keyFrameCreateLayerPrev = makeAction("key-frame-create-layer-prev", tr("Create Layer on Previous Key Frame")).icon("keyframe-previous").shortcut("Ctrl+Shift+E");
	QAction *keyFrameCreateGroup = makeAction("key-frame-create-group", tr("Create Group on Current Key Frame")).icon("keyframe-add").noDefaultShortcut();
	QAction *keyFrameCreateGroupNext = makeAction("key-frame-create-group-next", tr("Create Group on Next Key Frame")).icon("keyframe-next").noDefaultShortcut();
	QAction *keyFrameCreateGroupPrev = makeAction("key-frame-create-group-prev", tr("Create Group on Previous Key Frame")).icon("keyframe-previous").noDefaultShortcut();
	QAction *keyFrameDuplicateNext = makeAction("key-frame-duplicate-next", tr("Duplicate to Next Key Frame")).icon("keyframe-next").shortcut("Ctrl+Alt+Shift+T");
	QAction *keyFrameDuplicatePrev = makeAction("key-frame-duplicate-prev", tr("Duplicate to Previous Key Frame")).icon("keyframe-previous").shortcut("Ctrl+Alt+Shift+E");

	QActionGroup *layerKeyFrameGroup = new QActionGroup{this};
	layerKeyFrameGroup->addAction(keyFrameCreateLayer);
	layerKeyFrameGroup->addAction(keyFrameCreateLayerNext);
	layerKeyFrameGroup->addAction(keyFrameCreateLayerPrev);
	layerKeyFrameGroup->addAction(keyFrameCreateGroup);
	layerKeyFrameGroup->addAction(keyFrameCreateGroupNext);
	layerKeyFrameGroup->addAction(keyFrameCreateGroupPrev);
	layerKeyFrameGroup->addAction(keyFrameDuplicateNext);
	layerKeyFrameGroup->addAction(keyFrameDuplicatePrev);

	QMenu *animationMenu = menuBar()->addMenu(tr("&Animation"));
	animationMenu->addAction(showFlipbook);
	animationMenu->addAction(frameCountSet);
	animationMenu->addAction(framerateSet);
	animationMenu->addSeparator();
	animationMenu->addAction(keyFrameSetLayer);
	animationMenu->addAction(keyFrameSetEmpty);
	animationMenu->addAction(keyFrameCut);
	animationMenu->addAction(keyFrameCopy);
	animationMenu->addAction(keyFramePaste);
	animationMenu->addAction(keyFrameProperties);
	animationMenu->addAction(keyFrameDelete);
	animationMenu->addSeparator();
	animationMenu->addAction(keyFrameExposureIncrease);
	animationMenu->addAction(keyFrameExposureDecrease);
	animationMenu->addSeparator();
	QMenu *animationLayerMenu = animationMenu->addMenu(
		QIcon::fromTheme("layer-visible-on"), tr("Create Layer on Key Frame"));
	animationLayerMenu->addAction(keyFrameCreateLayer);
	animationLayerMenu->addAction(keyFrameCreateLayerNext);
	animationLayerMenu->addAction(keyFrameCreateLayerPrev);
	QMenu *animationGroupMenu = animationMenu->addMenu(
		QIcon::fromTheme("folder"), tr("Create Group on Key Frame"));
	animationGroupMenu->addAction(keyFrameCreateGroup);
	animationGroupMenu->addAction(keyFrameCreateGroupNext);
	animationGroupMenu->addAction(keyFrameCreateGroupPrev);
	QMenu *animationDuplicateMenu = animationMenu->addMenu(
		QIcon::fromTheme("edit-copy"), tr("Duplicate Key Frame"));
	animationDuplicateMenu->addAction(keyFrameDuplicateNext);
	animationDuplicateMenu->addAction(keyFrameDuplicatePrev);
	animationMenu->addSeparator();
	animationMenu->addAction(trackAdd);
	animationMenu->addAction(trackDuplicate);
	animationMenu->addAction(trackRetitle);
	animationMenu->addAction(trackDelete);
	animationMenu->addAction(trackVisible);
	animationMenu->addAction(trackOnionSkin);
	animationMenu->addSeparator();
	animationMenu->addAction(frameNext);
	animationMenu->addAction(framePrev);
	animationMenu->addAction(keyFrameNext);
	animationMenu->addAction(keyFramePrev);
	animationMenu->addAction(trackAbove);
	animationMenu->addAction(trackBelow);

	m_currentdoctools->addAction(showFlipbook);
	m_dockLayers->setLayerEditActions({layerAdd, groupAdd, layerDupe, layerMerge, layerProperties, layerDelete, layerSetFillSource, keyFrameSetLayer, keyFrameCreateLayer, keyFrameCreateLayerNext, keyFrameCreateLayerPrev, keyFrameCreateGroup, keyFrameCreateGroupNext, keyFrameCreateGroupPrev, keyFrameDuplicateNext, keyFrameDuplicatePrev, layerKeyFrameGroup});
	m_dockTimeline->setActions({keyFrameSetLayer, keyFrameSetEmpty, keyFrameCreateLayer, keyFrameCreateLayerNext, keyFrameCreateLayerPrev, keyFrameCreateGroup, keyFrameCreateGroupNext, keyFrameCreateGroupPrev, keyFrameDuplicateNext, keyFrameDuplicatePrev, keyFrameCut, keyFrameCopy, keyFramePaste, keyFrameProperties, keyFrameDelete, keyFrameExposureIncrease, keyFrameExposureDecrease, trackAdd, trackVisible, trackOnionSkin, trackDuplicate, trackRetitle, trackDelete, frameCountSet, framerateSet, frameNext, framePrev, keyFrameNext, keyFramePrev, trackAbove, trackBelow, animationLayerMenu, animationGroupMenu, animationDuplicateMenu}, layerViewNormal, layerViewCurrentFrame, showFlipbook);

	connect(showFlipbook, &QAction::triggered, this, &MainWindow::showFlipbook);

	//
	// Session menu
	//
	QAction *host = makeAction("hostsession", tr("&Host...")).statusTip(tr("Share your canvas with others")).noDefaultShortcut().icon("network-server");
	QAction *invite = makeAction("invitesession", tr("&Invite...")).statusTip(tr("Invite another user to this session")).noDefaultShortcut().icon("resource-group-new").disabled();
	QAction *join = makeAction("joinsession", tr("&Join...")).statusTip(tr("Join another user's drawing session")).noDefaultShortcut().icon("network-connect");
	QAction *browse = makeAction("browsesession", tr("&Browse...")).statusTip(tr("Browse session listings")).noDefaultShortcut().icon("edit-find");
	QAction *logout = makeAction("leavesession", tr("&Leave")).statusTip(tr("Leave this drawing session")).noDefaultShortcut().icon("network-disconnect").disabled();

	QAction *serverlog = makeAction("viewserverlog", tr("Event Log")).noDefaultShortcut();
	QAction *sessionSettings = makeAction("sessionsettings", tr("Settings...")).statusTip(tr("Change session settings, permissions, announcements and bans")).icon("configure").noDefaultShortcut().menuRole(QAction::NoRole).disabled();
	QAction *sessionUndoDepthLimit = makeAction("sessionundodepthlimit", tr("Undo Limit…")).noDefaultShortcut().disabled();

	QAction *gainop = makeAction("gainop", tr("Become Operator...")).noDefaultShortcut().disabled();
	QAction *resetsession = makeAction("resetsession", tr("&Reset...")).noDefaultShortcut().disabled();
	QAction *terminatesession = makeAction("terminatesession", tr("Terminate")).noDefaultShortcut();
	QAction *reportabuse = makeAction("reportabuse", tr("Report...")).noDefaultShortcut().disabled();

	QAction *locksession = makeAction("locksession", tr("Lock Everything")).statusTip(tr("Prevent changes to the canvas")).shortcut("F12").checkable();

	m_admintools->addAction(locksession);
	terminatesession->setEnabled(false);
	m_admintools->setEnabled(false);

	connect(host, &QAction::triggered, this, &MainWindow::host);
	connect(this, &MainWindow::hostSessionEnabled, host, &QAction::setEnabled);
	connect(invite, &QAction::triggered, this, &MainWindow::invite);
	connect(join, &QAction::triggered, this, &MainWindow::join);
	connect(browse, &QAction::triggered, this, &MainWindow::browse);
	connect(logout, &QAction::triggered, this, &MainWindow::leave);
	connect(sessionSettings, &QAction::triggered, m_sessionSettings, [this](){
		utils::showWindow(m_sessionSettings, shouldShowDialogMaximized());
	});
	connect(sessionUndoDepthLimit, &QAction::triggered, this, &MainWindow::changeUndoDepthLimit);
	connect(serverlog, &QAction::triggered, m_serverLogDialog, [this](){
		utils::showWindow(m_serverLogDialog, shouldShowDialogMaximized());
	});
	connect(reportabuse, &QAction::triggered, this, &MainWindow::reportAbuse);
	connect(gainop, &QAction::triggered, this, &MainWindow::tryToGainOp);
	connect(locksession, &QAction::triggered, m_doc, &Document::sendLockSession);

	connect(m_doc, &Document::sessionOpwordChanged, this, [gainop, this](bool hasOpword) {
		gainop->setEnabled(hasOpword && !m_doc->canvas()->aclState()->amOperator());
	});

	connect(resetsession, &QAction::triggered, this, &MainWindow::resetSession);
	connect(terminatesession, &QAction::triggered, this, &MainWindow::terminateSession);

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host);
	sessionmenu->addAction(invite);
	sessionmenu->addAction(join);
	sessionmenu->addAction(browse);
	sessionmenu->addAction(logout);
	sessionmenu->addSeparator();

	QMenu *modmenu = sessionmenu->addMenu(tr("Moderation"));
	modmenu->addAction(gainop);
	modmenu->addAction(terminatesession);
	modmenu->addAction(reportabuse);

	sessionmenu->addAction(resetsession);
	sessionmenu->addSeparator();
	sessionmenu->addAction(serverlog);
	sessionmenu->addAction(sessionSettings);
	sessionmenu->addAction(sessionUndoDepthLimit);
	sessionmenu->addAction(locksession);

	m_chatbox->setActions(invite, sessionSettings);

	//
	// Tools menu and toolbar
	//
	m_freehandAction = makeAction("toolbrush", tr("Freehand")).icon("draw-brush").statusTip(tr("Freehand brush tool")).shortcut("B").checkable();
	QAction *erasertool = makeAction("tooleraser", tr("Eraser")).icon("draw-eraser").statusTip(tr("Freehand eraser brush")).shortcut("E").checkable();
	QAction *linetool = makeAction("toolline", tr("&Line")).icon("draw-line").statusTip(tr("Draw straight lines")).shortcut("U").checkable();
	QAction *recttool = makeAction("toolrect", tr("&Rectangle")).icon("draw-rectangle").statusTip(tr("Draw unfilled squares and rectangles")).shortcut("R").checkable();
	QAction *ellipsetool = makeAction("toolellipse", tr("&Ellipse")).icon("draw-ellipse").statusTip(tr("Draw unfilled circles and ellipses")).shortcut("O").checkable();
	QAction *beziertool = makeAction("toolbezier", tr("Bezier Curve")).icon("draw-bezier-curves").statusTip(tr("Draw bezier curves")).shortcut("Ctrl+B").checkable();
	QAction *filltool = makeAction("toolfill", tr("&Flood Fill")).icon("fill-color").statusTip(tr("Fill areas")).shortcut("F").checkable();
	QAction *annotationtool = makeAction("tooltext", tr("&Annotation")).icon("draw-text").statusTip(tr("Add text to the picture")).shortcut("A").checked();

	QAction *pickertool = makeAction("toolpicker", tr("&Color Picker")).icon("color-picker").statusTip(tr("Pick colors from the image")).shortcut("I").checkable();
	QAction *lasertool = makeAction("toollaser", tr("&Laser Pointer")).icon("cursor-arrow").statusTip(tr("Point out things on the canvas")).shortcut("L").checkable();
	QAction *selectiontool = makeAction("toolselectrect", tr("&Select (Rectangular)")).icon("select-rectangular").statusTip(tr("Select area for copying")).shortcut("S").checkable();
	QAction *lassotool = makeAction("toolselectpolygon", tr("&Select (Free-Form)")).icon("edit-select-lasso").statusTip(tr("Select a free-form area for copying")).shortcut("D").checkable();
	QAction *transformtool = makeAction("tooltransform", tr("&Transform Tool")).icon("transform-move").statusTip(tr("Transform selection")).noDefaultShortcut().checkable();
	QAction *pantool = makeAction("toolpan", tr("Pan")).icon("hand").statusTip(tr("Pan canvas view")).shortcut("P").checkable();
	QAction *zoomtool = makeAction("toolzoom", tr("Zoom")).icon("edit-find").statusTip(tr("Zoom the canvas view")).shortcut("Z").checkable();
	QAction *inspectortool = makeAction("toolinspector", tr("Inspector")).icon("help-whatsthis").statusTip(tr("Find out who did it")).shortcut("Ctrl+I").checkable();

	m_drawingtools->addAction(m_freehandAction);
	m_drawingtools->addAction(erasertool);
	m_drawingtools->addAction(linetool);
	m_drawingtools->addAction(recttool);
	m_drawingtools->addAction(ellipsetool);
	m_drawingtools->addAction(beziertool);
	m_drawingtools->addAction(filltool);
	m_drawingtools->addAction(annotationtool);
	m_drawingtools->addAction(pickertool);
	m_drawingtools->addAction(lasertool);
	m_drawingtools->addAction(selectiontool);
	m_drawingtools->addAction(lassotool);
	m_drawingtools->addAction(transformtool);
	m_drawingtools->addAction(pantool);
	m_drawingtools->addAction(zoomtool);
	m_drawingtools->addAction(inspectortool);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(m_drawingtools->actions());

	QMenu *toolshortcuts = toolsmenu->addMenu(tr("&Shortcuts"));

	QMenu *devtoolsmenu = toolsmenu->addMenu(tr("Developer Tools"));
	QAction *systeminfo = makeAction("systeminfo", tr("System Information…")).noDefaultShortcut();
	QAction *tableteventlog = makeAction("tableteventlog", tr("Tablet Event Log...")).noDefaultShortcut();
	QAction *profile = makeAction("profile", tr("Profile...")).noDefaultShortcut();
	QAction *artificialLag = makeAction("artificiallag", tr("Set Artificial Lag...")).noDefaultShortcut();
	QAction *artificialDisconnect = makeAction("artificialdisconnect", tr("Artifical Disconnect...")).noDefaultShortcut();
	QAction *debugDump = makeAction("debugdump", tr("Record Debug Dumps")).checkable().noDefaultShortcut();
	QAction *openDebugDump = makeAction("opendebugdump", tr("Open Debug Dump...")).noDefaultShortcut();
	QAction *showNetStats = makeAction("shownetstats", tr("Statistics…")).noDefaultShortcut();
	devtoolsmenu->addAction(systeminfo);
	devtoolsmenu->addAction(tableteventlog);
	devtoolsmenu->addAction(profile);
	devtoolsmenu->addAction(artificialLag);
	devtoolsmenu->addAction(artificialDisconnect);
	devtoolsmenu->addAction(debugDump);
	devtoolsmenu->addAction(openDebugDump);
	devtoolsmenu->addAction(showNetStats);
	connect(devtoolsmenu, &QMenu::aboutToShow, this, &MainWindow::updateDevToolsActions);
	connect(systeminfo, &QAction::triggered, this, &MainWindow::showSystemInfo);
	connect(tableteventlog, &QAction::triggered, this, &MainWindow::toggleTabletEventLog);
	connect(profile, &QAction::triggered, this, &MainWindow::toggleProfile);
	connect(artificialLag, &QAction::triggered, this, &MainWindow::setArtificialLag);
	connect(artificialDisconnect, &QAction::triggered, this, &MainWindow::setArtificialDisconnect);
	connect(debugDump, &QAction::triggered, this, &MainWindow::toggleDebugDump);
	connect(openDebugDump, &QAction::triggered, this, &MainWindow::openDebugDump);
	connect(showNetStats, &QAction::triggered, m_netstatus, &widgets::NetStatus::showNetStats);

	QAction *currentEraseMode = makeAction("currenterasemode", tr("Toggle Eraser Mode")).shortcut("Ctrl+E");
	QAction *currentRecolorMode = makeAction("currentrecolormode", tr("Toggle Recolor Mode")).shortcut("Shift+E");
	QAction *changeForegroundColor = makeAction("chnageforegroundcolor", widgets::DualColorButton::foregroundText()).statusTip(tr("Choose the current foreground color")).noDefaultShortcut();
	QAction *changeBackgroundColor = makeAction("changebackgroundcolor", widgets::DualColorButton::backgroundText()).statusTip(tr("Choose the current background color")).noDefaultShortcut();
	QAction *swapcolors = makeAction("swapcolors", widgets::DualColorButton::swapText()).statusTip(tr("Swap current foreground and background color with each other")).shortcut("X");
	QAction *resetcolors = makeAction("resetcolors", widgets::DualColorButton::resetText()).statusTip(tr("Set foreground color to black and background color to white")).noDefaultShortcut();
	QAction *smallerbrush = makeAction("ensmallenbrush", tr("&Decrease Brush Size")).shortcut(Qt::Key_BracketLeft).autoRepeat();
	QAction *biggerbrush = makeAction("embiggenbrush", tr("&Increase Brush Size")).shortcut(Qt::Key_BracketRight).autoRepeat();
	QAction *reloadPreset = makeAction("reloadpreset", tr("&Reload Last Brush Preset")).shortcut("Shift+P");

	smallerbrush->setAutoRepeat(true);
	biggerbrush->setAutoRepeat(true);

	connect(currentEraseMode, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::toggleEraserMode);
	connect(currentRecolorMode, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::toggleRecolorMode);
	connect(changeForegroundColor, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::changeForegroundColor);
	connect(changeBackgroundColor, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::changeBackgroundColor);
	connect(swapcolors, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::swapColors);
	connect(resetcolors, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::resetColors);

	connect(smallerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(false); });
	connect(biggerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(true); });
	connect(reloadPreset, &QAction::triggered, m_dockToolSettings->brushSettings(), &tools::BrushSettings::reloadPreset);

	toolshortcuts->addAction(currentEraseMode);
	toolshortcuts->addAction(currentRecolorMode);
	toolshortcuts->addAction(changeForegroundColor);
	toolshortcuts->addAction(changeBackgroundColor);
	toolshortcuts->addAction(swapcolors);
	toolshortcuts->addAction(resetcolors);
	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);
	toolshortcuts->addAction(reloadPreset);

	m_toolBarDraw = new QToolBar(tr("Drawing tools"));
	m_toolBarDraw->setObjectName("drawtoolsbar");
	toggletoolbarmenu->addAction(m_toolBarDraw->toggleViewAction());

	m_freehandButton = new QToolButton(this);
	m_freehandButton->setCheckable(true);
	m_freehandButton->setChecked(m_freehandAction->isChecked());
	updateFreehandToolButton(tools::BrushSettings::NormalMode);
	connect(
		m_freehandAction, &QAction::toggled, m_freehandButton,
		&QAbstractButton::setChecked);
	connect(
		m_freehandButton, &QAbstractButton::clicked, this,
		&MainWindow::handleFreehandToolButtonClicked);
	connect(
		m_dockToolSettings->brushSettings(),
		&tools::BrushSettings::brushModeChanged, this,
		&MainWindow::updateFreehandToolButton);

	for(QAction *dt : m_drawingtools->actions()) {
		// Add a separator before color picker to separate brushes from non-destructive tools
		if(dt == pickertool) {
			m_toolBarDraw->addSeparator();
		}
		// Special button for the freehand tool to show erase and lock state.
		if(dt == m_freehandAction) {
			m_toolBarDraw->addWidget(m_freehandButton);
		} else {
			m_toolBarDraw->addAction(dt);
		}
	}

	m_smallScreenSpacer = new QWidget;
	m_smallScreenSpacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
	m_toolBarFile->insertWidget(m_toolBarFile->actions()[0], m_smallScreenSpacer);
	resetDefaultToolbars();

	//
	// Window menu (Mac only)
	//
#ifdef Q_OS_MACOS
	menuBar()->addMenu(MacMenu::instance()->windowMenu());
#endif

	//
	// Help menu
	//
	QAction *homepage = makeAction("dphomepage", tr("&Homepage")).statusTip(cmake_config::website()).noDefaultShortcut();
	QAction *tablettester = makeAction("tablettester", tr("Tablet Tester")).noDefaultShortcut();
	QAction *touchtester = makeAction("touchtester", tr("Touch Tester")).noDefaultShortcut();
	QAction *showlogfile = makeAction("showlogfile", tr("Log File")).noDefaultShortcut();
	QAction *about = makeAction("dpabout", tr("&About Drawpile")).menuRole(QAction::AboutRole).noDefaultShortcut();
	QAction *aboutqt = makeAction("aboutqt", tr("About &Qt")).menuRole(QAction::AboutQtRole).noDefaultShortcut();
#ifndef __EMSCRIPTEN__
	QAction *versioncheck = makeAction("versioncheck", tr("Check For Updates")).noDefaultShortcut();
#endif

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);

#ifndef __EMSCRIPTEN__
	connect(
		versioncheck, &QAction::triggered, this, &MainWindow::checkForUpdates);
#endif

	connect(tablettester, &QAction::triggered, [this]() {
		dialogs::TabletTestDialog *ttd=nullptr;
		// Check if dialog is already open
		for(QWidget *toplevel : qApp->topLevelWidgets()) {
			ttd = qobject_cast<dialogs::TabletTestDialog*>(toplevel);
			if(ttd)
				break;
		}
		if(!ttd) {
			ttd = new dialogs::TabletTestDialog;
			ttd->setAttribute(Qt::WA_DeleteOnClose);
		}
		utils::showWindow(ttd, shouldShowDialogMaximized());
		ttd->raise();
	});

	connect(touchtester, &QAction::triggered, [this] {
		dialogs::TouchTestDialog *ttd = nullptr;
		for(QWidget *toplevel : qApp->topLevelWidgets()) {
			ttd = qobject_cast<dialogs::TouchTestDialog*>(toplevel);
			if(ttd) {
				break;
			}
		}
		if(!ttd) {
			ttd = new dialogs::TouchTestDialog;
			ttd->setAttribute(Qt::WA_DeleteOnClose);
		}
		utils::showWindow(ttd, shouldShowDialogMaximized());
		ttd->raise();
	});

	connect(showlogfile, &QAction::triggered, [this] {
		QString logFilePath = utils::logFilePath();
		QFile logFile{logFilePath};
		if(!logFile.exists()) {
			utils::showWarning(
				this, tr("Missing Log File"),
				tr("Log file doesn't exist, do you need to enable logging in the preferences?"));
			return;
		}

#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
		if(logFile.open(QIODevice::ReadOnly)) {
			QString defaultName =
				QStringLiteral("drawpile-log-%1-%2.txt")
					.arg(cmake_config::version())
					.arg(QDateTime::currentDateTime().toString("yyyyMMddHHMMSS"));
			QString error;
			if(!FileWrangler(this).saveLogFile(
					defaultName, logFile.readAll(), &error)) {
				utils::showWarning(
					this, tr("Error Saving Log File"),
					tr("Could not write log file: %1").arg(error));
			}
		} else {
			utils::showWarning(
				this, tr("Error Saving Log File"),
				tr("Could not read log file: %1").arg(logFile.errorString()));
		}
#else
		QDesktopServices::openUrl(QUrl::fromLocalFile(utils::logFilePath()));
#endif
	});

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addAction(tablettester);
	helpmenu->addAction(touchtester);
	helpmenu->addAction(showlogfile);
#ifndef Q_OS_MACOS
	// Qt shunts the About menus into the Application menu on macOS, so this
	// would cause two separators to be placed instead of one
	helpmenu->addSeparator();
#endif
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);
	helpmenu->addSeparator();
#ifndef __EMSCRIPTEN__
	helpmenu->addAction(versioncheck);
#endif

	// Brush slot shortcuts

	m_brushSlots = new QActionGroup(this);
	for(int i=0;i<6;++i) {
		QAction *q = new QAction(tr("Brush slot #%1").arg(i+1), this);
		q->setAutoRepeat(false);
		q->setObjectName(QString("quicktoolslot-%1").arg(i));
		q->setShortcut(QKeySequence(QString::number(i+1)));
		q->setProperty("toolslotidx", i);
		CustomShortcutModel::registerCustomizableAction(q->objectName(), q->text(), q->shortcut(), QKeySequence());
		m_brushSlots->addAction(q);
		addAction(q);
		// Swapping with the eraser slot doesn't make sense.
		if(i != 5) {
			QAction *s = new QAction{tr("Swap With Brush Slot #%1").arg(i +1 ), this};
			s->setAutoRepeat(false);
			s->setObjectName(QStringLiteral("swapslot%1").arg(i));
			CustomShortcutModel::registerCustomizableAction(
				s->objectName(), s->text(), QKeySequence(), QKeySequence());
			addAction(s);
			connect(s, &QAction::triggered, this, [this, i] {
				m_dockToolSettings->brushSettings()->swapWithSlot(i);
			});
		}
	}
	connect(m_brushSlots, &QActionGroup::triggered, this, [this](QAction *a) {
		m_dockToolSettings->setToolSlot(a->property("toolslotidx").toInt());
		m_toolChangeTime.start();
	});

	// Color swatch shortcuts
	for(int i = 0; i < docks::ToolSettings::LASTUSED_COLOR_COUNT; ++i) {
		QAction *swatchAction = makeAction(
			qUtf8Printable(QStringLiteral("swatchcolor%1").arg(i)),
			tr("Swatch Color %1").arg(i + 1)).noDefaultShortcut();
		connect(swatchAction, &QAction::triggered, m_dockToolSettings, [=] {
			m_dockToolSettings->setLastUsedColor(i);
		});
	}

	// Add temporary tool change shortcut detector
	for(QAction *act : m_drawingtools->actions())
		act->installEventFilter(m_tempToolSwitchShortcut);

	for(QAction *act : m_brushSlots->actions())
		act->installEventFilter(m_tempToolSwitchShortcut);

	// Other shortcuts
	QAction *finishStrokeShortcut = makeAction("finishstroke", tr("Finish action")).shortcut(Qt::Key_Return);
	connect(finishStrokeShortcut, &QAction::triggered,
			m_doc->toolCtrl(), &tools::ToolController::finishMultipartDrawing);

	QAction *escapeShortcut = makeAction("cancelaction", tr("Cancel action")).shortcut(Qt::Key_Escape);
	connect(escapeShortcut, &QAction::triggered,
			m_doc->toolCtrl(), &tools::ToolController::cancelMultipartDrawing);

	QAction *focusCanvas = makeAction("focuscanvas", tr("Focus canvas")).shortcut(CTRL_KEY | Qt::Key_Tab);
	connect(focusCanvas, &QAction::triggered, this, [this]{
		m_canvasView->viewWidget()->setFocus();
	});

	const QList<QAction *> globalDockActions = {sideTabDocks, hideDocks, hideDockTitleBars};
	for(auto *dw : findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if(auto *titlebar = qobject_cast<docks::TitleWidget *>(dw->titleBarWidget())) {
			titlebar->addGlobalDockActions(globalDockActions);
		}
	}

	for(QObject *child : findChildren<QObject *>()) {
		if(qobject_cast<QAction *>(child)) {
			child->installEventFilter(this);
		}
	}

	if(m_singleSession) {
		QActionGroup *singleGroup = new QActionGroup(this);
		singleGroup->setExclusive(false);
		singleGroup->setEnabled(false);
		singleGroup->setVisible(false);
		singleGroup->addAction(newdocument);
		singleGroup->addAction(open);
		singleGroup->addAction(start);
		singleGroup->addAction(importOldAnimation);
		singleGroup->addAction(host);
		singleGroup->addAction(browse);
	}

	updateInterfaceModeActions();
}

void MainWindow::updateInterfaceModeActions()
{
	m_desktopModeActions->setEnabled(!m_smallScreenMode);
	m_desktopModeActions->setVisible(!m_smallScreenMode);
	m_smallScreenModeActions->setEnabled(m_smallScreenMode);
	m_smallScreenModeActions->setVisible(m_smallScreenMode);
	bool haveSmallScreenEditActions = !m_smallScreenEditActions.isEmpty();
	if(m_smallScreenMode && !haveSmallScreenEditActions) {
		m_smallScreenEditActions.append(m_toolBarEdit->addSeparator());
		QAction *viewflip = getAction("viewflip");
		m_toolBarEdit->addAction(viewflip);
		m_smallScreenEditActions.append(viewflip);
		QAction *viewmirror = getAction("viewmirror");
		m_toolBarEdit->addAction(viewmirror);
		m_smallScreenEditActions.append(viewmirror);
		m_smallScreenEditActions.append(m_toolBarEdit->addSeparator());
		QAction *zoomorig = getAction("zoomone");
		m_toolBarEdit->addAction(zoomorig);
		m_smallScreenEditActions.append(zoomorig);
		QAction *rotateorig = getAction("rotatezero");
		m_toolBarEdit->addAction(rotateorig);
		m_smallScreenEditActions.append(rotateorig);
	} else if(!m_smallScreenMode && haveSmallScreenEditActions) {
		for(QAction *action : m_smallScreenEditActions) {
			m_toolBarEdit->removeAction(action);
			if(action->isSeparator()) {
				delete action;
			}
		}
		m_smallScreenEditActions.clear();
	}
}

void MainWindow::reenableUpdates()
{
	setUpdatesEnabled(true);
	// Qt will inherit the update enabled state when restoring floating widgets,
	// but won't when re-enabling them on the parent. Gotta do it ourselves.
	for(QWidget *widget : findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
		if(!widget->updatesEnabled()) {
			widget->setUpdatesEnabled(true);
		}
	}
}

void MainWindow::keepCanvasPosition(const std::function<void()> &block)
{
	QWidget *canvasWidget = m_canvasView->viewWidget();
	QPoint centralPosBefore = centralWidget()->pos();
	QSize canvasSizeBefore = canvasWidget->size();
	block();
	QPoint centralPosDelta = centralWidget()->pos() - centralPosBefore;
	QSize canvasSizeDelta = canvasWidget->size() - canvasSizeBefore;
	emit viewShifted(
		centralPosDelta.x() + canvasSizeDelta.width() / 2.0,
		centralPosDelta.y() + canvasSizeDelta.height() / 2.0);
}

void MainWindow::createDocks()
{
	Q_ASSERT(m_doc);
	Q_ASSERT(m_canvasView);

	setDockNestingEnabled(true);

	// Create tool settings
	m_dockToolSettings = new docks::ToolSettings(m_doc->toolCtrl(), this);
	m_dockToolSettings->setObjectName("ToolSettings");
	m_dockToolSettings->setAllowedAreas(Qt::AllDockWidgetAreas);

	// Create brush palette
	m_dockBrushPalette = new docks::BrushPalette(this);
	m_dockBrushPalette->setObjectName("BrushPalette");
	m_dockBrushPalette->setAllowedAreas(Qt::AllDockWidgetAreas);

	m_dockBrushPalette->connectBrushSettings(
		m_dockToolSettings->brushSettings());

	// Create color docks
	//: "Wheel" refers to the color wheel.
	m_dockColorSpinner = new docks::ColorSpinnerDock(tr("Wheel"), this);
	m_dockColorSpinner->setObjectName("colorspinnerdock");
	m_dockColorSpinner->setAllowedAreas(Qt::AllDockWidgetAreas);

	m_dockColorPalette = new docks::ColorPaletteDock(tr("Palette"), this);
	m_dockColorPalette->setObjectName("colorpalettedock");
	m_dockColorPalette->setAllowedAreas(Qt::AllDockWidgetAreas);

	//: "Sliders" refers to the RGB and HSV sliders.
	m_dockColorSliders = new docks::ColorSliderDock(tr("Sliders"), this);
	m_dockColorSliders->setObjectName("colorsliderdock");
	m_dockColorSliders->setAllowedAreas(Qt::AllDockWidgetAreas);

	// Create layer list
	m_dockLayers = new docks::LayerList(this);
	m_dockLayers->setObjectName("LayerList");
	m_dockLayers->setAllowedAreas(Qt::AllDockWidgetAreas);

	// Create navigator
	m_dockNavigator = new docks::Navigator(this);
	m_dockNavigator->setObjectName("navigatordock");
	m_dockNavigator->setAllowedAreas(Qt::AllDockWidgetAreas);

	// Create timeline
	m_dockTimeline = new docks::Timeline(this);
	m_dockTimeline->setObjectName("Timeline");
	m_dockTimeline->setAllowedAreas(Qt::AllDockWidgetAreas);

	// Create onion skin settings
	m_dockOnionSkins = new docks::OnionSkinsDock(tr("Onion Skins"), this);
	m_dockOnionSkins->setObjectName("onionskins");
	m_dockOnionSkins->setAllowedAreas(Qt::AllDockWidgetAreas);
}

void MainWindow::resetDefaultDocks()
{
	addDockWidget(Qt::LeftDockWidgetArea, m_dockToolSettings);
	m_dockToolSettings->show();
	addDockWidget(Qt::LeftDockWidgetArea, m_dockBrushPalette);
	m_dockBrushPalette->show();
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSpinner);
	m_dockColorSpinner->show();
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorPalette);
	m_dockColorPalette->show();
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSliders);
	m_dockColorSliders->show();
	tabifyDockWidget(m_dockColorPalette, m_dockColorSliders);
	tabifyDockWidget(m_dockColorSliders, m_dockColorSpinner);
	addDockWidget(Qt::RightDockWidgetArea, m_dockLayers);
	m_dockLayers->show();
	addDockWidget(Qt::RightDockWidgetArea, m_dockNavigator);
	m_dockNavigator->hide(); // hidden by default
	addDockWidget(Qt::TopDockWidgetArea, m_dockTimeline);
	m_dockTimeline->show();
	addDockWidget(Qt::TopDockWidgetArea, m_dockOnionSkins);
	m_dockOnionSkins->show();
	if(m_smallScreenMode) {
		tabifyDockWidget(m_dockTimeline, m_dockOnionSkins);
	}
}

void MainWindow::resetDefaultToolbars()
{
	m_smallScreenSpacer->setVisible(m_smallScreenMode);
	if(m_smallScreenMode) {
		addToolBar(Qt::BottomToolBarArea, m_toolBarEdit);
		addToolBar(Qt::BottomToolBarArea, m_toolBarFile);
		addToolBar(Qt::LeftToolBarArea, m_toolBarDraw);
	} else {
		addToolBar(Qt::TopToolBarArea, m_toolBarFile);
		addToolBar(Qt::TopToolBarArea, m_toolBarEdit);
		addToolBar(Qt::TopToolBarArea, m_toolBarDraw);
	}
	m_toolBarFile->show();
	m_toolBarEdit->show();
	m_toolBarDraw->show();
}

bool MainWindow::isInitialSmallScreenMode()
{
	const desktop::settings::Settings &settings = dpApp().settings();
	switch(settings.interfaceMode()) {
	case int(desktop::settings::InterfaceMode::Desktop):
		return false;
	case int(desktop::settings::InterfaceMode::SmallScreen):
		return true;
	default:
		break;
	}

	bool useScreenSize;
#ifdef SINGLE_MAIN_WINDOW
	useScreenSize = true;
#else
	useScreenSize = settings.lastWindowMaximized();
#endif

	QSize s;
	if(useScreenSize) {
		QScreen *screen = QGuiApplication::primaryScreen();
		if(screen) {
			s = screen->availableSize();
		}
	} else {
		s = settings.lastWindowSize();
	}
	return isSmallScreenModeSize(s);
}

void MainWindow::updateInterfaceMode()
{
	if(!m_updatingInterfaceMode &&
	   !findChild<dialogs::LayoutsDialog *>(
		   "layoutsdialog", Qt::FindDirectChildrenOnly)) {
		QScopedValueRollback<bool> rollback(m_updatingInterfaceMode, true);
		const desktop::settings::Settings &settings = dpApp().settings();
		bool smallScreenMode = shouldUseSmallScreenMode(settings);
		if(smallScreenMode && !m_smallScreenMode) {
			switchInterfaceMode(true);
		} else if(!smallScreenMode && m_smallScreenMode) {
			switchInterfaceMode(false);
		}
	}
}

bool MainWindow::shouldUseSmallScreenMode(
	const desktop::settings::Settings &settings)
{
	switch(settings.interfaceMode()) {
	case int(desktop::settings::InterfaceMode::Desktop):
		return false;
	case int(desktop::settings::InterfaceMode::SmallScreen):
		return true;
	default:
		return isSmallScreenModeSize(size());
	}
}

bool MainWindow::isSmallScreenModeSize(const QSize &s)
{
	return s.isEmpty() || s.width() < DESKTOP_MODE_MIN_WIDTH ||
		   s.height() < DESKTOP_MODE_MIN_HEIGHT;
}

void MainWindow::switchInterfaceMode(bool smallScreenMode)
{
	setUpdatesEnabled(false);
	saveSplitterState();
	saveWindowState();
	m_smallScreenMode = smallScreenMode;

	QList<QDockWidget *> dockWidgets =
		findChildren<QDockWidget *>(QString(), Qt::FindDirectChildrenOnly);
	if(smallScreenMode) {
		if(m_hiddenDockState.isEmpty()) {
			m_desktopModeState = saveState();
		} else {
			m_desktopModeState.clear();
			m_desktopModeState.swap(m_hiddenDockState);
		}

		setFreezeDocks(false);
		for(QDockWidget *dw : dockWidgets) {
			dw->setFloating(false);
			dw->show();
			removeDockWidget(dw);
		}
		removeToolBar(m_toolBarFile);
		removeToolBar(m_toolBarEdit);
		m_splitter->setHandleWidth(0);
		m_chatbox->setSmallScreenMode(true);
		m_chatbox->hide();
		m_toolBarDraw->show();
		m_viewStatusBar->show();
		m_viewstatus->setHidden(true);
		setFreezeDocks(true);
		updateSideTabDocks();

		resetDefaultDocks();
		resetDefaultToolbars();
		initDefaultDocks();
		for(QDockWidget *dw : dockWidgets) {
			dw->hide();
		}
	} else {
		m_splitter->setHandleWidth(m_splitterOriginalHandleWidth);
		m_chatbox->show();
		m_chatbox->setSmallScreenMode(false);
		m_viewstatus->setHidden(false);
		m_viewStatusBar->show();
		updateSideTabDocks();

		QByteArray stateToRestore;
		stateToRestore.swap(m_desktopModeState);
		if(stateToRestore.isEmpty()) {
			stateToRestore = dpApp().settings().lastWindowState();
		}
		if(stateToRestore.isEmpty()) {
			setFreezeDocks(false);
			for(QDockWidget *dw : dockWidgets) {
				dw->setFloating(false);
				dw->show();
				removeDockWidget(dw);
			}
			removeToolBar(m_toolBarFile);
			removeToolBar(m_toolBarEdit);
			removeToolBar(m_toolBarDraw);
			resetDefaultDocks();
			resetDefaultToolbars();
			initDefaultDocks();
		} else {
			restoreState(stateToRestore);
		}

		setFreezeDocks(getAction("freezedocks")->isChecked());
	}

	m_canvasView->setShowToggleItems(smallScreenMode);
	updateInterfaceModeActions();
	reenableUpdates();
}

bool MainWindow::shouldShowDialogMaximized() const
{
#ifdef SINGLE_MAIN_WINDOW
	return m_smallScreenMode;
#else
	return m_smallScreenMode && isMaximized();
#endif
}
