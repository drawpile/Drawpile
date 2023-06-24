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
#include <QClipboard>
#include <QFile>
#include <QWindow>
#include <QVBoxLayout>
#include <QTimer>
#include <QTextEdit>
#include <QThreadPool>
#include <QKeySequence>
#include <QJsonDocument>

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
#include "desktop/scene/canvasview.h"
#include "desktop/scene/canvasscene.h"
#include "desktop/scene/selectionitem.h"
#include "libclient/canvas/userlist.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/canvas/documentmetadata.h"

#include "desktop/utils/recentfiles.h"
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

#include "desktop/widgets/viewstatus.h"
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
#include "libclient/parentalcontrols/parentalcontrols.h"

#include "libclient/tools/toolcontroller.h"
#include "desktop/toolwidgets/brushsettings.h"
#include "desktop/toolwidgets/colorpickersettings.h"
#include "desktop/toolwidgets/fillsettings.h"
#include "desktop/toolwidgets/selectionsettings.h"
#include "desktop/toolwidgets/annotationsettings.h"
#include "desktop/toolwidgets/lasersettings.h"
#include "desktop/toolwidgets/zoomsettings.h"
#include "desktop/toolwidgets/inspectorsettings.h"

#include "desktop/filewrangler.h"
#include "libclient/export/animationsaverrunnable.h"
#include "libshared/record/reader.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/drawdance/perf.h"

#include "desktop/dialogs/colordialog.h"
#include "desktop/dialogs/newdialog.h"
#include "desktop/dialogs/hostdialog.h"
#include "desktop/dialogs/joindialog.h"
#include "desktop/dialogs/layoutsdialog.h"
#include "desktop/dialogs/logindialog.h"
#include "desktop/dialogs/settingsdialog.h"
#include "desktop/dialogs/resizedialog.h"
#include "desktop/dialogs/playbackdialog.h"
#include "desktop/dialogs/dumpplaybackdialog.h"
#include "desktop/dialogs/flipbook.h"
#include "desktop/dialogs/resetdialog.h"
#include "desktop/dialogs/resetnoticedialog.h"
#include "desktop/dialogs/sessionsettings.h"
#include "desktop/dialogs/serverlogdialog.h"
#include "desktop/dialogs/tablettester.h"
#include "desktop/dialogs/abusereport.h"
#include "desktop/dialogs/brushsettingsdialog.h"
#include "desktop/dialogs/sessionundodepthlimitdialog.h"
#include "desktop/dialogs/userinfodialog.h"

#ifdef ENABLE_VERSION_CHECK
#include "desktop/dialogs/versioncheckdialog.h"
#endif

#ifdef Q_OS_WIN
#include "desktop/bundled/kis_tablet/kis_tablet_support_win.h"
#endif

using desktop::settings::Settings;
// Totally arbitrary nonsense
constexpr auto DEBOUNCE_MS = 250;

MainWindow::MainWindow(bool restoreWindowPosition)
	: QMainWindow(),
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
	  m_view(nullptr),
	  m_viewStatusBar(nullptr),
	  m_lockstatus(nullptr),
	  m_netstatus(nullptr),
	  m_viewstatus(nullptr),
	  m_statusChatButton(nullptr),
	  m_playbackDialog(nullptr),
	  m_dumpPlaybackDialog(nullptr),
	  m_sessionSettings(nullptr),
	  m_serverLogDialog(nullptr),
	  m_canvasscene(nullptr),
	  m_recentMenu(nullptr),
	  m_lastLayerViewMode(nullptr),
	  m_currentdoctools(nullptr),
	  m_admintools(nullptr),
	  m_modtools(nullptr),
	  m_canvasbgtools(nullptr),
	  m_resizetools(nullptr),
	  m_putimagetools(nullptr),
	  m_undotools(nullptr),
	  m_drawingtools(nullptr),
	  m_brushSlots(nullptr),
	  m_dockToggles(nullptr),
	  m_lastToolBeforePaste(-1),
#ifndef Q_OS_ANDROID
	  m_fullscreenOldMaximized(false),
#endif
	  m_tempToolSwitchShortcut(nullptr),
	  m_titleBarsHidden(false),
	  m_wasSessionLocked(false),
	  m_doc(nullptr),
	  m_exitAfterSave(false)
{
	// The document (initially empty)
	m_doc = new Document(dpApp().settings(), this);

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

	mainwinlayout->addWidget(m_splitter);

	// Create custom status bar
	m_viewStatusBar = new QStatusBar;
	m_viewStatusBar->setSizeGripEnabled(false);
	mainwinlayout->addWidget(m_viewStatusBar);

	// Create status indicator widgets
	m_viewstatus = new widgets::ViewStatus(this);

	m_netstatus = new widgets::NetStatus(this);
	m_lockstatus = new QLabel(this);
	m_lockstatus->setFixedSize(QSize(16, 16));

	// Statusbar chat button: this is normally hidden and only shown
	// when there are unread chat messages.
	m_statusChatButton = new QToolButton(this);
	m_statusChatButton->setAutoRaise(true);
	m_statusChatButton->setIcon(QIcon::fromTheme("drawpile_chat"));
	m_statusChatButton->hide();
	m_viewStatusBar->addWidget(m_statusChatButton);

	// Statusbar session size label
	QLabel *sessionHistorySize = new QLabel(this);
	m_viewStatusBar->addWidget(sessionHistorySize);

	m_viewStatusBar->addPermanentWidget(m_viewstatus);
	m_viewStatusBar->addPermanentWidget(m_netstatus);
	m_viewStatusBar->addPermanentWidget(m_lockstatus);

	int SPLITTER_WIDGET_IDX = 0;

	// Create canvas view (first splitter item)
	m_view = new widgets::CanvasView(this);
	m_splitter->addWidget(m_view);
	m_splitter->setCollapsible(SPLITTER_WIDGET_IDX++, false);

	// Create the chatbox
	m_chatbox = new widgets::ChatBox(m_doc, this);
	m_splitter->addWidget(m_chatbox);

	connect(m_chatbox, &widgets::ChatBox::reattachNowPlease, this, [this]() {
		m_splitter->addWidget(m_chatbox);
	});

	// Nice initial division between canvas and chat
	{
		const int h = height();
		m_splitter->setSizes(QList<int>() << (h * 2 / 3) << (h / 3));
	}

	// Create canvas scene
	m_canvasscene = new drawingboard::CanvasScene(this);
	m_canvasscene->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	m_view->setCanvas(m_canvasscene);

	// Create docks
	createDocks();

	// Crete persistent dialogs
	m_sessionSettings = new dialogs::SessionSettingsDialog(m_doc, this);
	m_serverLogDialog = new dialogs::ServerLogDialog(this);
	m_serverLogDialog->setModel(m_doc->serverLog());

	// Document <-> Main window connections
	connect(m_doc, &Document::canvasChanged, this, &MainWindow::onCanvasChanged);
	connect(m_doc, &Document::canvasSaveStarted, this, &MainWindow::onCanvasSaveStarted);
	connect(m_doc, &Document::canvasSaved, this, &MainWindow::onCanvasSaved);
	connect(m_doc, &Document::templateExported, this, &MainWindow::onTemplateExported);
	connect(m_doc, &Document::dirtyCanvas, this, &MainWindow::setWindowModified);
	connect(m_doc, &Document::sessionTitleChanged, this, &MainWindow::updateTitle);
	connect(m_doc, &Document::currentFilenameChanged, this, &MainWindow::updateTitle);
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

	connect(m_dockToolSettings->laserPointerSettings(), &tools::LaserPointerSettings::pointerTrackingToggled,
		m_view, &widgets::CanvasView::setPointerTracking);
	connect(m_dockToolSettings->zoomSettings(), &tools::ZoomSettings::resetZoom,
		this, [this]() { m_view->setZoom(100.0); });
	connect(m_dockToolSettings->zoomSettings(), &tools::ZoomSettings::fitToWindow,
		m_view, &widgets::CanvasView::zoomToFit);

	connect(m_dockToolSettings->brushSettings(), &tools::BrushSettings::brushSettingsDialogRequested,
		this, &MainWindow::showBrushSettingsDialog);

	connect(m_dockLayers, &docks::LayerList::layerSelected, this, &MainWindow::updateLockWidget);
	connect(m_dockLayers, &docks::LayerList::activeLayerVisibilityChanged, this, &MainWindow::updateLockWidget);

	connect(m_dockToolSettings, &docks::ToolSettings::sizeChanged, m_view, &widgets::CanvasView::setOutlineSize);
	connect(m_dockToolSettings, &docks::ToolSettings::subpixelModeChanged, m_view, &widgets::CanvasView::setOutlineMode);
	connect(m_view, &widgets::CanvasView::colorDropped, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(m_view, SIGNAL(imageDropped(QImage)), this, SLOT(pasteImage(QImage)));
	connect(m_view, &widgets::CanvasView::urlDropped, this, &MainWindow::dropUrl);
	connect(m_view, &widgets::CanvasView::viewTransformed, m_viewstatus, &widgets::ViewStatus::setTransformation);
	connect(m_view, &widgets::CanvasView::viewTransformed, m_dockNavigator, &docks::Navigator::setViewTransformation);

	connect(m_view, &widgets::CanvasView::viewRectChange, m_viewstatus, [this]() {
		const int min = qMin(int(m_view->fitToWindowScale() * 100), 100);
		m_viewstatus->setMinimumZoom(min);
		m_dockNavigator->setMinimumZoom(min);
	});

	connect(m_viewstatus, &widgets::ViewStatus::zoomChanged, m_view, &widgets::CanvasView::setZoom);
	connect(m_viewstatus, &widgets::ViewStatus::angleChanged, m_view, &widgets::CanvasView::setRotation);
	connect(m_dockNavigator, &docks::Navigator::zoomChanged, m_view, &widgets::CanvasView::setZoom);

	connect(m_dockToolSettings, &docks::ToolSettings::toolChanged, this, &MainWindow::toolChanged);
	connect(m_dockToolSettings, &docks::ToolSettings::activeBrushChanged, this, &MainWindow::updateLockWidget);

	// Color docks
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorPalette, &docks::ColorPaletteDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorSpinner, &docks::ColorSpinnerDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::foregroundColorChanged, m_dockColorSliders, &docks::ColorSliderDock::setColor);
	connect(m_dockToolSettings, &docks::ToolSettings::lastUsedColorsChanged, m_dockColorSpinner, &docks::ColorSpinnerDock::setLastUsedColors);
	connect(m_dockToolSettings, &docks::ToolSettings::lastUsedColorsChanged, m_dockColorSliders, &docks::ColorSliderDock::setLastUsedColors);
	connect(m_dockColorPalette, &docks::ColorPaletteDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(m_dockColorSpinner, &docks::ColorSpinnerDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(m_dockColorSliders, &docks::ColorSliderDock::colorSelected, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);

	// Canvas view -> canvas item, so that the item knows what are to re-render.
	connect(m_view, &widgets::CanvasView::viewRectChange, m_canvasscene, &drawingboard::CanvasScene::canvasViewportChanged);

	// Navigator <-> View
	connect(m_dockNavigator, &docks::Navigator::focusMoved, m_view, &widgets::CanvasView::scrollTo);
	connect(m_view, &widgets::CanvasView::viewRectChange, m_dockNavigator, &docks::Navigator::setViewFocus);
	connect(m_dockNavigator, &docks::Navigator::wheelZoom, m_view, &widgets::CanvasView::zoomSteps);


	// Network client <-> UI connections
	connect(m_view, &widgets::CanvasView::pointerMoved, m_doc, &Document::sendPointerMove);

	connect(m_doc, &Document::catchupProgress, m_netstatus, &widgets::NetStatus::setCatchupProgress);

	connect(m_doc->client(), &net::Client::serverStatusUpdate, sessionHistorySize, [sessionHistorySize](int size) {
		sessionHistorySize->setText(QString("%1 MB").arg(size / float(1024*1024), 0, 'f', 2));
	});

	connect(m_chatbox, &widgets::ChatBox::message, m_doc->client(), &net::Client::sendMessage);
	connect(m_dockTimeline, &docks::Timeline::timelineEditCommands, m_doc->client(), &net::Client::sendMessages);

	connect(m_serverLogDialog, &dialogs::ServerLogDialog::opCommand, m_doc->client(), &net::Client::sendMessage);
	connect(m_dockLayers, &docks::LayerList::layerCommands, m_doc->client(), &net::Client::sendMessages);

	connect(m_doc->client(), &net::Client::userInfoRequested, this, &MainWindow::sendUserInfo);

	// Tool controller <-> UI connections
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged, m_canvasscene, &drawingboard::CanvasScene::setActiveAnnotation);
	connect(m_doc->toolCtrl(), &tools::ToolController::colorUsed, m_dockToolSettings, &docks::ToolSettings::addLastUsedColor);
	connect(m_doc->toolCtrl(), &tools::ToolController::zoomRequested, m_view, &widgets::CanvasView::zoomTo);
	connect(m_doc->toolCtrl(), &tools::ToolController::busyStateChanged, this, [this](bool busy) {
		m_view->setBusy(busy);
		setDrawingToolsEnabled(!busy);
	});

	connect(m_canvasscene, &drawingboard::CanvasScene::annotationDeleted, this, [this](int id) {
		if(m_doc->toolCtrl()->activeAnnotation() == id)
			m_doc->toolCtrl()->setActiveAnnotation(0);
	});

	connect(m_doc->toolCtrl(), &tools::ToolController::toolCursorChanged, m_view, &widgets::CanvasView::setToolCursor);
	m_view->setToolCursor(m_doc->toolCtrl()->activeToolCursor());
	connect(m_doc->toolCtrl(), &tools::ToolController::toolCapabilitiesChanged, m_view, &widgets::CanvasView::setToolCapabilities);
	m_view->setToolCapabilities(
		m_doc->toolCtrl()->activeToolAllowColorPick(),
		m_doc->toolCtrl()->activeToolAllowToolAdjust(),
		m_doc->toolCtrl()->activeToolHandlesRightClick());

	connect(m_view, &widgets::CanvasView::penDown, m_doc->toolCtrl(), &tools::ToolController::startDrawing);
	connect(m_view, &widgets::CanvasView::penMove, m_doc->toolCtrl(), &tools::ToolController::continueDrawing);
	connect(m_view, &widgets::CanvasView::penHover, m_doc->toolCtrl(), &tools::ToolController::hoverDrawing);
	connect(m_view, &widgets::CanvasView::penUp, m_doc->toolCtrl(), &tools::ToolController::endDrawing);
	connect(m_view, &widgets::CanvasView::quickAdjust, m_dockToolSettings, &docks::ToolSettings::quickAdjustCurrent1);

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

	connect(m_canvasscene, &drawingboard::CanvasScene::canvasResized, m_doc->toolCtrl(), &tools::ToolController::offsetActiveTool);

	connect(m_view, &widgets::CanvasView::reconnectRequested, this, [this]() {
		joinSession(m_doc->client()->sessionUrl(true));
	});

	// Network status changes
	connect(m_doc, &Document::serverConnected, this, &MainWindow::onServerConnected);
	connect(m_doc, &Document::serverLoggedIn, this, &MainWindow::onServerLogin);
	connect(m_doc, &Document::serverDisconnected, this, &MainWindow::onServerDisconnected);
	connect(m_doc, &Document::serverDisconnected, sessionHistorySize, [sessionHistorySize]() {
		sessionHistorySize->setText(QString());
	});
	connect(m_doc, &Document::compatibilityModeChanged, this, &MainWindow::onCompatibilityModeChanged);
	connect(m_doc, &Document::sessionNsfmChanged, this, &MainWindow::onNsfmChanged);

	connect(m_doc, &Document::serverConnected, m_netstatus, &widgets::NetStatus::connectingToHost);
	connect(m_doc->client(), &net::Client::serverDisconnecting, m_netstatus, &widgets::NetStatus::hostDisconnecting);
	connect(m_doc, &Document::serverDisconnected, m_netstatus, &widgets::NetStatus::hostDisconnected);
	connect(m_doc, &Document::sessionRoomcodeChanged, m_netstatus, &widgets::NetStatus::setRoomcode);

	connect(m_doc->client(), SIGNAL(bytesReceived(int)), m_netstatus, SLOT(bytesReceived(int)));
	connect(m_doc->client(), &net::Client::bytesSent, m_netstatus, &widgets::NetStatus::bytesSent);
	connect(m_doc->client(), &net::Client::lagMeasured, m_netstatus, &widgets::NetStatus::lagMeasured);
	connect(m_doc->client(), &net::Client::youWereKicked, m_netstatus, &widgets::NetStatus::kicked);
	connect(m_doc->client(), &net::Client::serverMessage, [=](const QString &message, bool alert) {
		if (alert && m_chatbox->isCollapsed()) {
			m_netstatus->message(message);
		}
	});

	connect(&dpApp(), &DrawpileApp::setDockTitleBarsHidden, this, &MainWindow::setDockTitleBarsHidden);

	// Create actions and menus
	setupActions();
	setDrawingToolsEnabled(false);
	m_dockToolSettings->triggerUpdate();

	// Restore settings
	readSettings(restoreWindowPosition);

	// Set status indicators
	updateLockWidget();
	setRecorderStatus(false);

#ifdef Q_OS_MACOS
	MacMenu::instance()->addWindow(this);
#endif

	// Show self
	updateTitle();
	utils::showWindow(this);

	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

#ifdef SINGLE_MAIN_WINDOW
	dpApp().deleteAllMainWindowsExcept(this);
#endif

	dpApp().settings().trySubmit();
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MACOS
	MacMenu::instance()->removeWindow(this);
#endif

	// Get rid of the selection here, otherwise it calls back into an
	// already-destroyed MainWindow and causes a crash.
	m_doc->cancelSelection();

	// Clear this out first so there will be no weird signals emitted
	// while the document is being torn down.
	m_view->setScene(nullptr);
	delete m_canvasscene;

	// Make sure all child dialogs are closed
	for(auto *child : findChildren<QDialog *>(QString(), Qt::FindDirectChildrenOnly)) {
		delete child;
	}

	dpApp().settings().trySubmit();
}

void MainWindow::onCanvasChanged(canvas::CanvasModel *canvas)
{
	m_canvasscene->initCanvas(canvas);

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

	connect(canvas, &canvas::CanvasModel::selectionRemoved, this, &MainWindow::selectionRemoved);

	connect(canvas, &canvas::CanvasModel::userJoined, this, [this](int, const QString &name) {
		m_viewStatusBar->showMessage(tr("🙋 %1 joined!").arg(name), 2000);
	});

	connect(m_serverLogDialog, &dialogs::ServerLogDialog::inspectModeChanged, canvas, QOverload<int>::of(&canvas::CanvasModel::inspectCanvas));
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

	m_dockToolSettings->fillSettings()->setLayerList(m_canvasscene->model()->layerlist());
	m_dockToolSettings->inspectorSettings()->setUserList(m_canvasscene->model()->userlist());

	// Make sure the UI matches the default feature access level
	m_currentdoctools->setEnabled(true);
	setDrawingToolsEnabled(true);
	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		DP_Feature f = DP_Feature(i);
		onFeatureAccessChange(f, m_doc->canvas()->aclState()->canUseFeature(f));
	}
	onUndoDepthLimitSet(canvas->paintEngine()->undoDepthLimit());
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

/**
 * Get either a new MainWindow or this one if replacable
 */
MainWindow *MainWindow::replaceableWindow()
{
	if(!canReplace()) {
		if(windowState().testFlag(Qt::WindowFullScreen))
			toggleFullscreen();
		saveWindowState();
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		return win;

	} else {
		return this;
	}
}

/**
 * The file is added to the list of recent files and the menus on all open
 * mainwindows are updated.
 * @param file filename to add
 */
void MainWindow::addRecentFile(const QString& file)
{
	RecentFiles::addFile(file);
	for(QWidget *widget : QApplication::topLevelWidgets()) {
		MainWindow *win = qobject_cast<MainWindow*>(widget);
		if(win)
			RecentFiles::initMenu(win->m_recentMenu);
	}
#ifdef Q_OS_MACOS
	MacMenu::instance()->updateRecentMenu();
#endif
}

/**
 * Set window title according to currently open file and session
 */
void MainWindow::updateTitle()
{
	QString name;
	if(m_doc->currentFilename().isEmpty()) {
		name = tr("Untitled");

	} else {
		const QFileInfo info(m_doc->currentFilename());
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
	m_drawingtools->setEnabled(enable && m_doc->canvas());
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
					shortcuts << v.value<QKeySequence>();
				} else {
					const auto list = v.toList();
					for(const auto &vv : list) {
						if(vv.canConvert<QKeySequence>())
							shortcuts << vv.value<QKeySequence>();
					}
				}
				a->setShortcuts(shortcuts);

			} else {
				a->setShortcut(CustomShortcutModel::getDefaultShortcut(name));
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

	settings.bindTabletEraser(m_dockToolSettings, [=](bool eraser) {
		if(eraser) {
			connect(&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings, &docks::ToolSettings::eraserNear, Qt::UniqueConnection);
		} else {
			disconnect(&dpApp(), &DrawpileApp::eraserNear, m_dockToolSettings, &docks::ToolSettings::eraserNear);
		}
	});

	settings.bindShareBrushSlotColor(m_dockToolSettings->brushSettings(), &tools::BrushSettings::setShareBrushSlotColor);

	// Restore previously used window size and position
	resize(settings.lastWindowSize());

	if(windowpos) {
		const auto pos = settings.lastWindowPosition();
		if (dpApp().primaryScreen()->availableGeometry().contains(pos))
			move(pos);
	}

	if(settings.lastWindowMaximized())
		setWindowState(Qt::WindowMaximized);

	// The following state restoration requires the window to be resized, but Qt
	// does that lazily on the next event loop iteration. So we forcefully flush
	// the event loop here to actually get the window resized before continuing.
	dpApp().processEvents();

	// Restore dock, toolbar and view states
	if(const auto lastWindowState = settings.lastWindowState(); !lastWindowState.isEmpty()) {
		restoreState(settings.lastWindowState());
	}
	if(const auto lastWindowViewState = settings.lastWindowViewState(); !lastWindowViewState.isEmpty()) {
		m_splitter->restoreState(settings.lastWindowViewState());
	}

	connect(m_splitter, &QSplitter::splitterMoved, this, [=] {
		m_saveSplitterDebounce.start(DEBOUNCE_MS);
	});

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

	// Customize shortcuts
	settings.bindShortcuts(this, &MainWindow::loadShortcuts);

	// Restore recent files
	RecentFiles::initMenu(m_recentMenu);

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
		m_saveWindowDebounce.start(DEBOUNCE_MS);
		break;
	case QEvent::Shortcut: {
		QShortcutEvent *shortcutEvent = static_cast<QShortcutEvent *>(event);
		if(shortcutEvent->isAmbiguous()) {
			showAmbiguousShortcutMessage(shortcutEvent);
			return true;
		}
		break;
	}
	default: {}
	}
	return QMainWindow::eventFilter(object, event);
}

void MainWindow::showAmbiguousShortcutMessage(QShortcutEvent *shortcutEvent)
{
	CustomShortcutModel shortcutsModel;
	shortcutsModel.loadShortcuts(dpApp().settings().shortcuts());
	QStringList matchingShortcuts;
	const QKeySequence &keySequence = shortcutEvent->key();
	for(const CustomShortcut &shortcut : shortcutsModel.getShortcutsMatching(keySequence)) {
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
	m_saveSplitterDebounce.stop();

	auto &settings = dpApp().settings();
	settings.setLastWindowViewState(m_splitter->saveState());
}

void MainWindow::saveWindowState()
{
	m_saveWindowDebounce.stop();

	auto &settings = dpApp().settings();
	settings.setLastWindowPosition(normalGeometry().topLeft());
	settings.setLastWindowSize(normalGeometry().size());
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

	m_dockToolSettings->saveSettings();
}

void MainWindow::requestUserInfo(int userId)
{
	net::Client *client = m_doc->client();
	QJsonObject info{{"type", "request_user_info"}};
	client->sendMessage(drawdance::Message::makeUserInfo(
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
		{"app_version", cmake_config::version()},
		{"protocol_version", DP_PROTOCOL_VERSION},
		{"qt_version", QString::number(QT_VERSION_MAJOR)},
		{"os", os},
		{"tablet_input", tabletinput::current()},
		{"tablet_mode", m_view->isTabletEnabled() ? "pressure" : "none"},
		{"touch_mode", m_view->isTouchDrawEnabled() ? "draw"
			: m_view->isTouchScrollEnabled() ? "scroll" : "none"},
		{"smoothing", m_doc->toolCtrl()->globalSmoothing()},
		{"pressure_curve", m_view->pressureCurve().toString()},
	};
	net::Client *client = m_doc->client();
	client->sendMessage(drawdance::Message::makeUserInfo(
		client->myId(), userId, QJsonDocument{info}));
}

/**
 * Confirm exit. A confirmation dialog is popped up if there are unsaved
 * changes or network connection is open.
 * @param event event info
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
	if(m_doc->isSaveInProgress()) {
		// Don't quit while save is in progress
		m_exitAfterSave = true;
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
			box.setWindowModality(Qt::WindowModal);

			const QPushButton *exitbtn = box.addButton(tr("Exit anyway"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Cancel"),
					QMessageBox::RejectRole);

			box.exec();
			if(box.clickedButton() == exitbtn) {
				m_doc->client()->disconnectFromServer();
			} else {
				event->ignore();
				return;
			}
		}

		// Then confirm unsaved changes
		if(isWindowModified()) {
			QMessageBox box(QMessageBox::Question, tr("Exit Drawpile"),
					tr("There are unsaved changes. Save them before exiting?"),
					QMessageBox::NoButton, this);
			box.setWindowModality(Qt::WindowModal);
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
				m_exitAfterSave = true;
				save();
			}

			// Cancel exit
			if(box.clickedButton() == cancelbtn || cancel) {
				event->ignore();
				return;
			}
		}
	}
	exit();
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
		m_saveWindowDebounce.start(DEBOUNCE_MS);
		break;
	case QEvent::ActivationChange:
		if(m_saveSplitterDebounce.isActive()) {
			saveSplitterState();
		}
		if(m_saveWindowDebounce.isActive()) {
			saveWindowState();
		}
		dpApp().settings().submit();
		break;
	default:
		break;
	}

	return QMainWindow::event(event);
}

/**
 * Show the "new document" dialog
 */
void MainWindow::showNew()
{
	auto dlg = new dialogs::NewDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::NewDialog::accepted, this, &MainWindow::newDocument);
	utils::showWindow(dlg);
}

void MainWindow::newDocument(const QSize &size, const QColor &background)
{
	MainWindow *w = replaceableWindow();
	w->m_doc->loadBlank(size, background);
}

/**
 * Open the selected file
 * @param file file to open
 * @pre file.isEmpty()!=false
 */
void MainWindow::open(const QUrl& url)
{
	MainWindow *w = replaceableWindow();
	if(w != this) {
		w->open(url);
		return;
	}

	if(url.isLocalFile()) {
		QString file = url.toLocalFile();
		static constexpr auto opt = QRegularExpression::CaseInsensitiveOption;
		if(QRegularExpression{"\\.dp(rec|txt)$", opt}.match(file).hasMatch()) {
			bool isTemplate;
			DP_LoadResult result = m_doc->loadRecording(file, false, &isTemplate);
			showLoadResultMessage(result);
			if(result == DP_LOAD_RESULT_SUCCESS && !isTemplate) {
				QFileInfo fileinfo(file);
				m_playbackDialog = new dialogs::PlaybackDialog(m_doc->canvas(), this);
				m_playbackDialog->setWindowTitle(fileinfo.completeBaseName() + " - " + m_playbackDialog->windowTitle());
				m_playbackDialog->setAttribute(Qt::WA_DeleteOnClose);
				m_playbackDialog->show();
				m_playbackDialog->centerOnParent();
			}
		} else if(QRegularExpression{"\\.drawdancedump$", opt}.match(file).hasMatch()) {
			DP_LoadResult result = m_doc->loadRecording(file, true);
			showLoadResultMessage(result);
			if(result == DP_LOAD_RESULT_SUCCESS) {
				QFileInfo fileinfo{file};
				m_dumpPlaybackDialog = new dialogs::DumpPlaybackDialog{m_doc->canvas(), this};
				m_dumpPlaybackDialog->setWindowTitle(QStringLiteral("%1 - %2")
					.arg(fileinfo.completeBaseName()).arg(m_dumpPlaybackDialog->windowTitle()));
				m_dumpPlaybackDialog->setAttribute(Qt::WA_DeleteOnClose);
				m_dumpPlaybackDialog->show();
			}

		} else {
			QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
			DP_LoadResult result = m_doc->loadFile(file);
			showLoadResultMessage(result);
			QApplication::restoreOverrideCursor();
			if(result) {
				getAction("hostsession")->setEnabled(true);
			}
		}

		addRecentFile(file);

	} else {
		auto *filedownload = new networkaccess::FileDownload(this);

		filedownload->setTarget(
			utils::paths::writablePath(
				QStandardPaths::DownloadLocation,
				".",
				url.fileName().isEmpty() ? QStringLiteral("drawpile-download") : url.fileName()
				)
			);

		connect(filedownload, &networkaccess::FileDownload::progress, m_netstatus, &widgets::NetStatus::setDownloadProgress);
		connect(filedownload, &networkaccess::FileDownload::finished, this, [this, filedownload](const QString &errorMessage) {
			m_netstatus->hideDownloadProgress();
			filedownload->deleteLater();

			if(errorMessage.isEmpty()) {
				QFile *f = static_cast<QFile*>(filedownload->file()); // this is guaranteed to be a QFile when we used setTarget()
				open(QUrl::fromLocalFile(f->fileName()));
			} else {
				showErrorMessage(errorMessage);
			}
		});

		filedownload->start(url);
	}
}

/**
 * Show a file selector dialog. If there are unsaved changes, open the file
 * in a new window.
 */
void MainWindow::open()
{
	QString filename = FileWrangler{this}.getOpenPath();
	QUrl url = QUrl::fromLocalFile(filename);
	if(url.isValid()) {
		open(url);
	}
}

/**
 * If no file name has been selected, \a saveas is called.
 */
void MainWindow::save()
{
	QString result = FileWrangler{this}.saveImage(m_doc);
	if(!result.isEmpty()) {
		addRecentFile(result);
	}
}

/**
 * A standard file dialog is used to get the name of the file to save.
 * If no suffix is the suffix from the current filter is used.
 */
void MainWindow::saveas()
{
	QString result = FileWrangler{this}.saveImageAs(m_doc);
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

void MainWindow::onCanvasSaveStarted()
{
	QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
	getAction("savedocument")->setEnabled(false);
	getAction("savedocumentas")->setEnabled(false);
	m_viewStatusBar->showMessage(tr("Saving..."));

	dialogs::ResetNoticeDialog *dlg = findChild<dialogs::ResetNoticeDialog *>(
		RESET_NOTICE_DIALOG_NAME, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->setSaveInProgress(true);
	}
}

void MainWindow::onCanvasSaved(const QString &errorMessage)
{
	QApplication::restoreOverrideCursor();
	getAction("savedocument")->setEnabled(true);
	getAction("savedocumentas")->setEnabled(true);

	dialogs::ResetNoticeDialog *dlg = findChild<dialogs::ResetNoticeDialog *>(
		RESET_NOTICE_DIALOG_NAME, Qt::FindDirectChildrenOnly);
	if(dlg) {
		dlg->setSaveInProgress(false);
	}

	setWindowModified(m_doc->isDirty());
	updateTitle();

	if(!errorMessage.isEmpty())
		showErrorMessageWithDetails(tr("Couldn't save image"), errorMessage);
	else
		m_viewStatusBar->showMessage(tr("Image saved"), 1000);

	// Cancel exit if canvas is modified while it was being saved
	if(m_doc->isDirty())
		m_exitAfterSave = false;

	if(m_exitAfterSave)
		close();
}

void MainWindow::showResetNoticeDialog(const drawdance::CanvasState &canvasState)
{
	dialogs::ResetNoticeDialog *dlg = findChild<dialogs::ResetNoticeDialog *>(
		RESET_NOTICE_DIALOG_NAME, Qt::FindDirectChildrenOnly);
	if(!dlg) {
		dlg = new dialogs::ResetNoticeDialog{
			canvasState, m_doc->isCompatibilityMode(), this};
		dlg->setObjectName(RESET_NOTICE_DIALOG_NAME);
		dlg->setAttribute(Qt::WA_DeleteOnClose);
		dlg->setSaveInProgress(m_doc->isSaveInProgress());
		connect(
			dlg, &dialogs::ResetNoticeDialog::saveRequested, this,
			&MainWindow::savePreResetImageAs);
		connect(
			m_doc, &Document::catchupProgress, dlg,
			&dialogs::ResetNoticeDialog::catchupProgress);
		dlg->show();
	}
}

void MainWindow::savePreResetImageAs(const drawdance::CanvasState &canvasState)
{
	QString result = FileWrangler{this}.savePreResetImageAs(m_doc, canvasState);
	if(!result.isEmpty()) {
		addRecentFile(result);
	}
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
	QString filename = FileWrangler{this}.getSaveGifPath();
	if(!filename.isEmpty()) {
		exportAnimation(filename, DP_save_animation_gif);
	}
}

#ifndef Q_OS_ANDROID
void MainWindow::exportAnimationFrames()
{
	QString dirname = FileWrangler{this}.getSaveAnimationFramesPath();
	if(!dirname.isEmpty()) {
		exportAnimation(dirname, DP_save_animation_frames);
	}
}
#endif

void MainWindow::exportAnimation(const QString &path, AnimationSaverRunnable::SaveFn saveFn)
{
	auto *progressDialog = new QProgressDialog(
		tr("Saving animation..."),
		tr("Cancel"),
		0,
		100,
		this);
	progressDialog->setMinimumDuration(500);

	AnimationSaverRunnable *saver = new AnimationSaverRunnable(
		m_doc->canvas()->paintEngine(),
		saveFn,
		path);

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
		fp = new dialogs::Flipbook{this};
		fp->setObjectName("flipbook");
		fp->setAttribute(Qt::WA_DeleteOnClose);
		fp->setPaintEngine(m_doc->canvas()->paintEngine());
		utils::showWindow(fp);
	}
}

void MainWindow::setRecorderStatus(bool on)
{
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
	if(drawdance::Perf::isOpen()) {
		if(!drawdance::Perf::close()) {
			showErrorMessageWithDetails(tr("Error closing profile."), DP_error());
		}
	} else {
		QString path = FileWrangler{this}.getSavePerformanceProfilePath();
		if(!path.isEmpty()) {
			if(!drawdance::Perf::open(path)) {
				showErrorMessageWithDetails(tr("Error opening profile."), DP_error());
			}
		}
	}
}

void MainWindow::toggleTabletEventLog()
{
	if(drawdance::EventLog::isOpen()) {
		if(!drawdance::EventLog::close()) {
			showErrorMessageWithDetails(tr("Error closing tablet event log."), DP_error());
		}
	} else {
		QString path = FileWrangler{this}.getSaveTabletEventLogPath();
		if(!path.isEmpty()) {
			if(drawdance::EventLog::open(path)) {
				DP_event_log_write_meta("Drawpile: %s", cmake_config::version());
				DP_event_log_write_meta("Qt: %s", QT_VERSION_STR);
				DP_event_log_write_meta("OS: %s", qUtf8Printable(QSysInfo::prettyProductName()));
				DP_event_log_write_meta("Input: %s", tabletinput::current());
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

	utils::showWindow(dlg);
	dlg->activateWindow();
	dlg->raise();
}

/**
 * The settings window will automatically destruct when it is closed.
 */
dialogs::SettingsDialog *MainWindow::showSettings()
{
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog;
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	utils::showWindow(dlg);
	return dlg;
}

void MainWindow::host()
{
	if(!m_doc->canvas()) {
		qWarning("No canvas!");
		return;
	}

	auto dlg = new dialogs::HostDialog(this);

	connect(dlg, &dialogs::HostDialog::finished, this, [this, dlg](int i) {
		if(i==QDialog::Accepted) {
			dlg->rememberSettings();
			hostSession(dlg);
		}
		dlg->deleteLater();
	});
	utils::showWindow(dlg);
}

void MainWindow::hostSession(dialogs::HostDialog *dlg)
{
	const bool useremote = !dlg->getRemoteAddress().isEmpty();
	QUrl address;

	if(useremote) {
		QString scheme;
		if(dlg->getRemoteAddress().startsWith("drawpile://")==false)
			scheme = "drawpile://";
		address = QUrl(scheme + dlg->getRemoteAddress(),
				QUrl::TolerantMode);

	} else {
		address.setHost(WhatIsMyIp::guessLocalAddress());
	}

	if(address.isValid() == false || address.host().isEmpty()) {
		utils::showWindow(dlg);
		showErrorMessage(tr("Invalid address"));
		return;
	}

	// Start server if hosting locally
	if(!useremote) {
#if 0 // FIXME
		auto *server = new server::BuiltinServer(
			m_doc->canvas()->stateTracker(),
			m_doc->canvas()->aclFilter(),
			this);

		QString errorMessage;
		if(!server->start(&errorMessage)) {
			QMessageBox::warning(this, tr("Host Session"), errorMessage);
			delete server;
			return;
		}

		connect(m_doc->client(), &net::Client::serverDisconnected, server, &server::BuiltinServer::stop);
		connect(m_doc->canvas()->stateTracker(), &canvas::StateTracker::softResetPoint, server, &server::BuiltinServer::doInternalReset);

		if(server->port() != DRAWPILE_PROTO_DEFAULT_PORT)
			address.setPort(server->port());
#endif
	}

	// Connect to server
	net::LoginHandler *login = new net::LoginHandler(
		useremote ? net::LoginHandler::Mode::HostRemote : net::LoginHandler::Mode::HostBuiltin,
		address,
		this);
	login->setUserId(m_doc->canvas()->localUserId());
	login->setSessionAlias(dlg->getSessionAlias());
	login->setPassword(dlg->getPassword());
	login->setTitle(dlg->getTitle());
	login->setAnnounceUrl(dlg->getAnnouncementUrl());
	login->setNsfm(dlg->isNsfm());
	if(useremote) {
		login->setInitialState(m_doc->canvas()->generateSnapshot(
			true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS));
	}

	utils::showWindow(new dialogs::LoginDialog(login, this));

	m_doc->client()->connectToServer(dpApp().settings().serverTimeout(), login);
}

/**
 * Show the join dialog
 */
void MainWindow::join(const QUrl &initialUrl)
{
	auto dlg = new dialogs::JoinDialog(initialUrl, this);

	connect(dlg, &dialogs::JoinDialog::finished, this, [this, dlg](int i) {
		if(i==QDialog::Accepted) {
			QUrl url = dlg->getUrl();

			if(!url.isValid()) {
				// TODO add validator to prevent this from happening
				showErrorMessage("Invalid address");
				return;
			}

			dlg->rememberSettings();

			joinSession(url, dlg->autoRecordFilename());
		}
		dlg->deleteLater();
	});
	utils::showWindow(dlg);
}

/**
 * Leave action triggered, ask for confirmation
 */
void MainWindow::leave()
{
	QMessageBox *leavebox = new QMessageBox(
		QMessageBox::Question,
		m_doc->sessionTitle().isEmpty() ? tr("Untitled") : m_doc->sessionTitle(),
		tr("Really leave the session?"),
		QMessageBox::NoButton,
		this,
		Qt::MSWindowsFixedSizeDialogHint|Qt::Sheet
	);
	leavebox->setAttribute(Qt::WA_DeleteOnClose);
	leavebox->addButton(tr("Leave"), QMessageBox::YesRole);
	leavebox->setDefaultButton(
			leavebox->addButton(tr("Stay"), QMessageBox::NoRole)
			);
	connect(leavebox, &QMessageBox::finished, this, [this](int result) {
		if(result == 0)
			m_doc->client()->disconnectFromServer();
	});

	if(m_doc->client()->uploadQueueBytes() > 0) {
		leavebox->setIcon(QMessageBox::Warning);
		leavebox->setInformativeText(tr("There is still unsent data! Please wait until transmission completes!"));
	}

	leavebox->show();
}

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

	utils::showWindow(dlg);
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
	dialogs::ResetDialog *dlg = new dialogs::ResetDialog(
		m_doc->canvas()->paintEngine(), m_doc->isCompatibilityMode(), this);
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
			canvas::CanvasModel *canvas = m_doc->canvas();
			if(canvas->aclState()->amOperator()) {
				drawdance::MessageList snapshot = dlg->getResetImage();
				canvas->amendSnapshotMetadata(
					snapshot, true, DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);
				m_doc->sendResetSession(snapshot);
			}
			dlg->deleteLater();
		});

	} else {
		dlg->setCanReset(false);
	}

	utils::showWindow(dlg);
}

void MainWindow::terminateSession()
{
	auto dlg = new QMessageBox(
		QMessageBox::Question,
		tr("Terminate session"),
		tr("Really terminate this session?"),
		QMessageBox::Ok|QMessageBox::Cancel,
		this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setDefaultButton(QMessageBox::Cancel);
	dlg->button(QMessageBox::Ok)->setText(tr("Terminate"));
	dlg->setWindowModality(Qt::WindowModal);

	connect(dlg, &QMessageBox::finished, this, [this](int res) {
		if(res == QMessageBox::Ok)
			m_doc->sendTerminateSession();
	});

	dlg->show();
}

/**
 * @param url URL
 */
void MainWindow::joinSession(const QUrl& url, const QString &autoRecordFile)
{
	if(!canReplace()) {
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		win->joinSession(url, autoRecordFile);
		return;
	}

	net::LoginHandler *login = new net::LoginHandler(
			net::LoginHandler::Mode::Join, url, this);
	auto *dlg = new dialogs::LoginDialog(login, this);
	connect(m_doc, &Document::catchupProgress, dlg, &dialogs::LoginDialog::catchupProgress);
	connect(m_doc, &Document::serverLoggedIn, dlg, [dlg,this](bool join) {
		dlg->onLoginDone(join);
		m_canvasscene->hideCanvas();
	});
	connect(dlg, &dialogs::LoginDialog::destroyed, m_canvasscene, &drawingboard::CanvasScene::showCanvas);
	connect(dlg, &dialogs::LoginDialog::destroyed, this, &MainWindow::showCompatibilityModeWarning);

	dlg->show();
	m_doc->setRecordOnConnect(autoRecordFile);
	m_doc->client()->connectToServer(dpApp().settings().serverTimeout(), login);
}

/**
 * Now connecting to server
 */
void MainWindow::onServerConnected()
{
	// Enable connection related actions
	getAction("hostsession")->setEnabled(false);
	getAction("leavesession")->setEnabled(true);
	getAction("sessionsettings")->setEnabled(true);

	// Disable UI until login completes
	m_view->setEnabled(false);
	setDrawingToolsEnabled(false);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::onServerDisconnected(const QString &message, const QString &errorcode, bool localDisconnect)
{
	getAction("hostsession")->setEnabled(m_doc->canvas() != nullptr);
	getAction("leavesession")->setEnabled(false);
	getAction("sessionsettings")->setEnabled(false);
	getAction("reportabuse")->setEnabled(false);
	m_admintools->setEnabled(false);
	m_modtools->setEnabled(false);
	m_sessionSettings->close();

	// Re-enable UI
	m_view->setEnabled(true);
	setDrawingToolsEnabled(true);

	// Display login error if not yet logged in
	if(!m_doc->client()->isLoggedIn() && !localDisconnect) {
		QMessageBox *msgbox = new QMessageBox(
			QMessageBox::Warning,
			QString(),
			tr("Could not connect to server"),
			QMessageBox::Ok,
			this
		);

		msgbox->setAttribute(Qt::WA_DeleteOnClose);
		msgbox->setWindowModality(Qt::WindowModal);
		msgbox->setInformativeText(message);

		if(errorcode == "SESSIONIDINUSE") {
			// We tried to host a session using with a vanity ID, but that
			// ID was taken. Show a button for quickly joining that session instead
			msgbox->setInformativeText(msgbox->informativeText() + "\n" + tr("Would you like to join the session instead?"));

			QAbstractButton *joinbutton = msgbox->addButton(tr("Join"), QMessageBox::YesRole);

			msgbox->removeButton(msgbox->button(QMessageBox::Ok));
			msgbox->addButton(QMessageBox::Cancel);

			QUrl url = m_doc->client()->sessionUrl(true);

			connect(joinbutton, &QAbstractButton::clicked, this, [this, url]() {
				joinSession(url);
			});

		}

		// Work around Qt macOS bug(?): if a window has more than two modal dialogs (sheets)
		// open at the same time (in this case, the login dialog that hasn't closed yet)
		// the main window will still be stuck after the dialogs close.
		QTimer::singleShot(1, msgbox, &QMessageBox::show); // NOLINT clang-tidy thinks this leaks
	}
	// If logged in but disconnected unexpectedly, show notification bar
	else if(m_doc->client()->isLoggedIn() && !localDisconnect) {
		m_view->showDisconnectedWarning(tr("Disconnected:") + " " + message);
	}
}

/**
 * Server connection established and login successfull
 */
void MainWindow::onServerLogin()
{
	m_netstatus->loggedIn(m_doc->client()->sessionUrl());
	m_netstatus->setSecurityLevel(m_doc->client()->securityLevel(), m_doc->client()->hostCertificate());
	m_view->setEnabled(true);
	m_sessionSettings->setPersistenceEnabled(m_doc->client()->serverSuppotsPersistence());
	m_sessionSettings->setAutoResetEnabled(m_doc->client()->sessionSupportsAutoReset());
	m_sessionSettings->setAuthenticated(m_doc->client()->isAuthenticated());
	setDrawingToolsEnabled(true);
	m_modtools->setEnabled(m_doc->client()->isModerator());
	getAction("reportabuse")->setEnabled(m_doc->client()->serverSupportsReports());
}

void MainWindow::onCompatibilityModeChanged(bool compatibilityMode)
{
	m_dockToolSettings->brushSettings()->setCompatibilityMode(compatibilityMode);
	m_dockToolSettings->selectionSettings()->setCompatibilityMode(compatibilityMode);
}

void MainWindow::updateLockWidget()
{
	canvas::CanvasModel *canvas = m_doc->canvas();
	canvas::AclState *aclState = canvas ? canvas->aclState() : nullptr;

	bool resetLocked = aclState && aclState->isResetLocked();
	bool locked = resetLocked;
	QString toolTip;
	if(locked) {
		toolTip = tr("Reset in progress");
	}

	bool sessionLocked = aclState && aclState->isSessionLocked();
	getAction("locksession")->setChecked(sessionLocked);
	if(!locked && sessionLocked) {
		locked = true;
		toolTip = tr("Board is locked");
	}

	if(sessionLocked && !m_wasSessionLocked) {
		notification::playSound(notification::Event::LOCKED);
	} else if(!sessionLocked && m_wasSessionLocked) {
		notification::playSound(notification::Event::UNLOCKED);
	}
	m_wasSessionLocked = sessionLocked;

	if(!locked && m_dockLayers->isCurrentLayerLocked()) {
		locked = true;
		toolTip = tr("Layer is locked");
	}

	if(!locked && m_dockToolSettings->isCurrentToolLocked()) {
		locked = true;
		toolTip = tr("Tool is locked");
	}

	if(locked) {
		m_lockstatus->setPixmap(QIcon::fromTheme("object-locked").pixmap(16, 16));
	} else {
		m_lockstatus->setPixmap(QPixmap());
	}
	m_lockstatus->setToolTip(toolTip);

	m_view->setLocked(locked);
	m_view->setResetInProgress(resetLocked);
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
}

void MainWindow::onUndoDepthLimitSet(int undoDepthLimit)
{
	QAction *action = getAction("sessionundodepthlimit");
	action->setProperty("undodepthlimit", undoDepthLimit);
	action->setText(tr("Undo Limit... (%1)").arg(undoDepthLimit));
	action->setStatusTip(tr("Change the session's undo limit, current limit is %1.").arg(undoDepthLimit));
}

/**
 * Write settings and exit. The application will not be terminated until
 * the last mainwindow is closed.
 */
void MainWindow::exit()
{
	if(windowState().testFlag(Qt::WindowFullScreen))
		toggleFullscreen();
	setDocksHidden(false);
	saveWindowState();
	deleteLater();
}

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
	if(message.isEmpty())
		return;

	QMessageBox *msgbox = new QMessageBox(
		QMessageBox::Warning,
		QString(),
		message, QMessageBox::Ok,
		this,
		Qt::Dialog|Qt::Sheet|Qt::MSWindowsFixedSizeDialogHint
	);
	msgbox->setAttribute(Qt::WA_DeleteOnClose);
	msgbox->setWindowModality(Qt::WindowModal);
	msgbox->setInformativeText(details);
	msgbox->show();
}

void MainWindow::showLoadResultMessage(DP_LoadResult result)
{
	switch(result) {
	case DP_LOAD_RESULT_SUCCESS:
		break;
    case DP_LOAD_RESULT_BAD_ARGUMENTS:
		showErrorMessage(tr("Bad arguments, this is probably a bug in Drawpile."));
		break;
    case DP_LOAD_RESULT_UNKNOWN_FORMAT:
		showErrorMessage(tr("Unsupported format."));
		break;
    case DP_LOAD_RESULT_OPEN_ERROR:
		showErrorMessageWithDetails(tr("Couldn't open file for reading."), DP_error());
		break;
    case DP_LOAD_RESULT_READ_ERROR:
		showErrorMessageWithDetails(tr("Error reading file."), DP_error());
		break;
	case DP_LOAD_RESULT_BAD_MIMETYPE:
		showErrorMessage(tr("File content doesn't match its type."));
		break;
	case DP_LOAD_RESULT_RECORDING_INCOMPATIBLE:
		showErrorMessage(tr("Incompatible recording."));
		break;
	default:
		showErrorMessageWithDetails(tr("Unknown error."), DP_error());
		break;
	}
}

void MainWindow::setShowAnnotations(bool show)
{
	QAction *annotationtool = getAction("tooltext");
	annotationtool->setEnabled(show);
	m_canvasscene->showAnnotations(show);
	if(!show) {
		if(annotationtool->isChecked())
			getAction("toolbrush")->trigger();
	}
}

void MainWindow::setShowLaserTrails(bool show)
{
	QAction *lasertool = getAction("toollaser");
	lasertool->setEnabled(show);
	m_canvasscene->showLaserTrails(show);
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
#ifndef Q_OS_ANDROID
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
}

void MainWindow::setDocksHidden(bool hidden)
{
	QPoint centerPosBefore = centralWidget()->pos();
	if(hidden) {
		m_hiddenDockState = saveState();
		for(auto *w : findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
			const auto shouldHide = w->isVisible()
				&& (w->inherits("QDockWidget") || w->inherits("QToolBar"));
			if (shouldHide) {
				w->hide();
			}
		}
		// Force recalculation of the central widget's position. Otherwise this
		// will happen lazily on the next repaint and we can't scroll properly.
		// Doing it this way is clearly a hack, but I can't figure out another
		// way to do it that doesn't introduce flicker or the window resizing.
		restoreState(saveState());
	} else {
		restoreState(m_hiddenDockState);
		m_hiddenDockState.clear();
	}

	m_viewStatusBar->setHidden(hidden);

	QPoint centerPosDelta = centralWidget()->pos() - centerPosBefore;
	m_view->scrollBy(centerPosDelta.x(), centerPosDelta.y());
	m_dockToggles->setDisabled(hidden);
}

void MainWindow::setDockTitleBarsHidden(bool hidden)
{
	QAction *freezeDocks = getAction("freezedocks");
	QAction *hideDockTitleBars = getAction("hidedocktitlebars");
	bool actuallyHidden = hidden && !freezeDocks->isChecked()
		&& hideDockTitleBars->isChecked();
	if(actuallyHidden != m_titleBarsHidden) {
		m_titleBarsHidden = hidden;
		for(auto *dw : findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
				dw->titleBarWidget()->setHidden(hidden);
		}
	}
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
		m_lastToolBeforePaste = -1;
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
	m_canvasscene->showAnnotationBorders(tool==tools::Tool::ANNOTATION);

	// Send pointer updates when using the laser pointer (TODO checkbox)
	m_view->setPointerTracking(
		tool == tools::Tool::LASERPOINTER &&
		m_dockToolSettings->laserPointerSettings()->pointerTracking());

	// Remove selection when not using selection tool
	if(tool != tools::Tool::SELECTION && tool != tools::Tool::POLYGONSELECTION)
		m_doc->selectNone();

	// Deselect annotation when tool changed
	if(tool != tools::Tool::ANNOTATION)
		m_doc->toolCtrl()->setActiveAnnotation(0);

	m_doc->toolCtrl()->setActiveTool(tool);
	updateLockWidget();
}

void MainWindow::selectionRemoved()
{
	if(m_lastToolBeforePaste>=0) {
		// Selection was just removed and we had just pasted an image
		// so restore the previously used tool
		QAction *toolaction = m_drawingtools->actions().at(m_lastToolBeforePaste);
		toolaction->trigger();
	}
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
	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if(mimeData->hasImage()) {
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
		if(pasteAtPos && m_view->isPointVisible(pastepos))
			pasteImage(mimeData->imageData().value<QImage>(), &pastepos, true);
		else
			pasteImage(mimeData->imageData().value<QImage>());
	}
}

void MainWindow::pasteCentered()
{
	const QMimeData *mimeData = QApplication::clipboard()->mimeData();
	if(mimeData->hasImage()) {
		pasteImage(mimeData->imageData().value<QImage>(), nullptr, true);
	}
}

void MainWindow::pasteFile()
{
	QString filename = FileWrangler{this}.getOpenPasteImagePath();
	QUrl url = QUrl::fromLocalFile(filename);
	if(url.isValid()) {
		pasteFile(url);
	}
}

void MainWindow::pasteFile(const QUrl &url)
{
	if(url.isLocalFile()) {
		QImage img(url.toLocalFile());
		if(img.isNull()) {
			showErrorMessage(tr("The image could not be loaded"));
			return;
		}

		pasteImage(img);
	} else {
		auto *filedownload = new networkaccess::FileDownload(this);

		filedownload->setExpectedType("image/");

		connect(filedownload, &networkaccess::FileDownload::progress, m_netstatus, &widgets::NetStatus::setDownloadProgress);
		connect(filedownload, &networkaccess::FileDownload::finished, this, [this, filedownload](const QString &errorMessage) {
			m_netstatus->hideDownloadProgress();
			filedownload->deleteLater();

			if(errorMessage.isEmpty()) {
				QImageReader reader(filedownload->file());
				QImage image = reader.read();

				if(image.isNull())
					showErrorMessage(reader.errorString());
				else
					pasteImage(image);

			} else {
				showErrorMessage(errorMessage);
			}
		});

		filedownload->start(url);
	}
}

void MainWindow::pasteImage(const QImage &image, const QPoint *point, bool force)
{
	if(!m_canvasscene->model()->aclState()->canUseFeature(DP_FEATURE_PUT_IMAGE))
		return;

	if(m_dockToolSettings->currentTool() != tools::Tool::SELECTION && m_dockToolSettings->currentTool() != tools::Tool::POLYGONSELECTION) {
		int currentTool = m_dockToolSettings->currentTool();
		getAction("toolselectrect")->trigger();
		m_lastToolBeforePaste = currentTool;
	}

	m_doc->pasteImage(image, point ? *point : m_view->viewCenterPoint(), force);
}

void MainWindow::dropUrl(const QUrl &url)
{
	if(m_canvasscene->hasImage())
		pasteFile(url);
	else
		open(url);
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
			drawdance::Message messages[] = {
				drawdance::Message::makeUndoPoint(contextId),
				drawdance::Message::makeAnnotationDelete(contextId, a),
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
	if(!m_doc->canvas()) {
		qWarning("resizeCanvas: no canvas!");
		return;
	}

	const QSize size = m_doc->canvas()->size();
	dialogs::ResizeDialog *dlg = new dialogs::ResizeDialog(size, this);
	dlg->setPreviewImage(m_doc->canvas()->paintEngine()->getPixmap().scaled(300, 300, Qt::KeepAspectRatio).toImage());
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	// Preset crop from selection if one exists
	if (m_doc->canvas()->selection()) {
		dlg->setBounds(m_doc->canvas()->selection()->boundingRect());
	}

	connect(dlg, &QDialog::accepted, this, [this, dlg]() {
		if (m_doc->canvas()->selection()) {
			m_doc->canvas()->setSelection(nullptr);
		}
		dialogs::ResizeVector r = dlg->resizeVector();
		if(!r.isZero()) {
			m_doc->sendResizeCanvas(r.top, r.right, r.bottom, r.left);
		}
	});
	utils::showWindow(dlg);
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
		QColor sessionColor = paintEngine->backgroundColor();
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
		m_doc->canvas()->paintEngine()->backgroundColor(), this);
	connect(dlg, &color_widgets::ColorDialog::colorSelected, m_doc, &Document::sendCanvasBackground);
	utils::showWindow(dlg);
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
		color = paintEngine->backgroundColor();
	}

	color_widgets::ColorDialog *dlg =
		dialogs::newDeleteOnCloseColorDialog(color, this);
	connect(
		dlg, &color_widgets::ColorDialog::colorSelected, paintEngine,
		&canvas::PaintEngine::setLocalBackgroundColor);
	utils::showWindow(dlg);
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
	if(!dlg) {
		dlg = new dialogs::LayoutsDialog{saveState(), this};
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
			m_doc->client()->sendMessage(drawdance::Message::makeUndoDepth(
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
	QString filename = FileWrangler{this}.getOpenDebugDumpsPath();
	QUrl url = QUrl::fromLocalFile(filename);
	if(url.isValid()) {
		open(url);
	}
}


void MainWindow::about()
{
	QMessageBox::about(nullptr, tr("About Drawpile"),
			QStringLiteral("<p><b>Drawpile %1</b><br>").arg(cmake_config::version()) +
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
			QStringLiteral("<hr><p><b>%1</b> %2</p><p><b>%3</b> %4</p>")
				.arg(tr("Settings File:"))
				.arg(dpApp().settings().path().toHtmlEscaped())
				.arg(tr("Tablet Input:"))
				.arg(QCoreApplication::translate("tabletinput", tabletinput::current()))
	);
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

	m_modtools = new QActionGroup(this);
	m_modtools->setEnabled(false);

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
			toggledockaction->shortcut());
		addAction(toggledockaction);
	}

	toggledockmenu->addSeparator();
	QAction *freezeDocks = makeAction("freezedocks", tr("Lock Docks")).noDefaultShortcut().checkable().remembered();
	toggledockmenu->addAction(freezeDocks);
	connect(freezeDocks, &QAction::toggled, this, &MainWindow::setFreezeDocks);

	QAction *sideTabDocks = makeAction("sidetabdocks", tr("Vertical Tabs on Sides")).noDefaultShortcut().checkable().remembered();
	toggledockmenu->addAction(sideTabDocks);
	auto updateSideTabDocks = [=](){
		if(sideTabDocks->isChecked()) {
			setDockOptions(dockOptions() | QMainWindow::VerticalTabs);
		} else {
			setDockOptions(dockOptions() & ~QMainWindow::VerticalTabs);
		}
	};
	connect(sideTabDocks, &QAction::toggled, updateSideTabDocks);
	updateSideTabDocks();

	QAction *hideDocks = makeAction("hidedocks", tr("Hide Docks")).checkable().shortcut("tab");
	toggledockmenu->addAction(hideDocks);
	connect(hideDocks, &QAction::toggled, this, &MainWindow::setDocksHidden);

	QAction *hideDockTitleBars = makeAction("hidedocktitlebars", tr("Hold Shift to Arrange")).noDefaultShortcut().checked().remembered();
	toggledockmenu->addAction(hideDockTitleBars);
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
	QAction *save = makeAction("savedocument", tr("&Save")).icon("document-save").shortcut(QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", tr("Save &As...")).icon("document-save-as").shortcut(QKeySequence::SaveAs);
	QAction *savesel = makeAction("saveselection", tr("Save Selection...")).icon("document-save-as").noDefaultShortcut();
	QAction *autosave = makeAction("autosave", tr("Autosave")).noDefaultShortcut().checkable().disabled();
	QAction *importBrushes = makeAction("importbrushes", tr("&Brushes...")).noDefaultShortcut();
	QAction *exportTemplate = makeAction("exporttemplate", tr("Session &Template...")).noDefaultShortcut();
	QAction *exportGifAnimation = makeAction("exportanimgif", tr("Animated &GIF...")).noDefaultShortcut();
#ifndef Q_OS_ANDROID
	QAction *exportAnimationFrames = makeAction("exportanimframes", tr("Animation &Frames...")).noDefaultShortcut();
#endif

	QAction *record = makeAction("recordsession", tr("Record...")).icon("media-record").noDefaultShortcut();
	QAction *quit = makeAction("exitprogram", tr("&Quit")).icon("application-exit").shortcut("Ctrl+Q").menuRole(QAction::QuitRole);

#ifdef Q_OS_MACOS
	m_currentdoctools->addAction(closefile);
#endif
	m_currentdoctools->addAction(save);
	m_currentdoctools->addAction(saveas);
	m_currentdoctools->addAction(exportTemplate);
	m_currentdoctools->addAction(savesel);
	m_currentdoctools->addAction(exportGifAnimation);
#ifndef Q_OS_ANDROID
	m_currentdoctools->addAction(exportAnimationFrames);
#endif
	m_currentdoctools->addAction(record);

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));
	connect(exportTemplate, &QAction::triggered, this, &MainWindow::exportTemplate);
	connect(importBrushes, &QAction::triggered, m_dockBrushPalette, &docks::BrushPalette::importMyPaintBrushes);
	connect(savesel, &QAction::triggered, this, &MainWindow::saveSelection);

	connect(autosave, &QAction::triggered, m_doc, &Document::setAutosave);
	connect(m_doc, &Document::autosaveChanged, autosave, &QAction::setChecked);
	connect(m_doc, &Document::canAutosaveChanged, autosave, &QAction::setEnabled);

	connect(exportGifAnimation, &QAction::triggered, this, &MainWindow::exportGifAnimation);
#ifndef Q_OS_ANDROID
	connect(exportAnimationFrames, &QAction::triggered, this, &MainWindow::exportAnimationFrames);
#endif
	connect(record, &QAction::triggered, this, &MainWindow::toggleRecording);

#ifdef Q_OS_MACOS
	connect(closefile, SIGNAL(triggered()), this, SLOT(close()));
	connect(quit, SIGNAL(triggered()), MacMenu::instance(), SLOT(quitAll()));
#else
	connect(quit, SIGNAL(triggered()), this, SLOT(close()));
#endif

	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(newdocument);
	filemenu->addAction(open);
	m_recentMenu = filemenu->addMenu(tr("Open &Recent"));
	filemenu->addSeparator();

#ifdef Q_OS_MACOS
	filemenu->addAction(closefile);
#endif
	filemenu->addAction(save);
	filemenu->addAction(saveas);
	filemenu->addAction(savesel);
	filemenu->addAction(autosave);
	filemenu->addSeparator();

	QMenu *importMenu = filemenu->addMenu(tr("&Import"));
	importMenu->setIcon(QIcon::fromTheme("document-import"));
	importMenu->addAction(importBrushes);

	QMenu *exportMenu = filemenu->addMenu(tr("&Export"));
	exportMenu->setIcon(QIcon::fromTheme("document-export"));
	exportMenu->addAction(exportTemplate);
	exportMenu->addAction(exportGifAnimation);
#ifndef Q_OS_ANDROID
	exportMenu->addAction(exportAnimationFrames);
#endif
	filemenu->addAction(record);
	filemenu->addSeparator();

	filemenu->addAction(quit);

	QToolBar *filetools = new QToolBar(tr("File Tools"));
	filetools->setObjectName("filetoolsbar");
	toggletoolbarmenu->addAction(filetools->toggleViewAction());
	filetools->addAction(newdocument);
	filetools->addAction(open);
	filetools->addAction(save);
	filetools->addAction(record);
	addToolBar(Qt::TopToolBarArea, filetools);

	connect(m_recentMenu, &QMenu::triggered, this, [this](QAction *action) {
		this->open(QUrl::fromLocalFile(action->property("filepath").toString()));
	});

	//
	// Edit menu
	//
	QAction *undo = makeAction("undo", tr("&Undo")).icon("edit-undo").shortcut(QKeySequence::Undo);
	QAction *redo = makeAction("redo", tr("&Redo")).icon("edit-redo").shortcut(QKeySequence::Redo);
	QAction *copy = makeAction("copyvisible", tr("&Copy Visible")).icon("edit-copy").statusTip(tr("Copy selected area to the clipboard")).shortcut("Shift+Ctrl+C");
	QAction *copyMerged = makeAction("copymerged", tr("Copy Merged")).icon("edit-copy").statusTip(tr("Copy selected area, excluding the background, to the clipboard")).shortcut("Ctrl+Alt+C");
	QAction *copylayer = makeAction("copylayer", tr("Copy &Layer")).icon("edit-copy").statusTip(tr("Copy selected area of the current layer to the clipboard")).shortcut(QKeySequence::Copy);
	QAction *cutlayer = makeAction("cutlayer", tr("Cu&t Layer")).icon("edit-cut").statusTip(tr("Cut selected area of the current layer to the clipboard")).shortcut(QKeySequence::Cut);
	QAction *paste = makeAction("paste", tr("&Paste")).icon("edit-paste").shortcut(QKeySequence::Paste);
	QAction *pasteCentered = makeAction("paste-centered", tr("Paste in View Center")).icon("edit-paste").shortcut("Ctrl+Shift+V");
	QAction *stamp = makeAction("stamp", tr("&Stamp")).shortcut("Ctrl+T");

	QAction *pastefile = makeAction("pastefile", tr("Paste &From File...")).icon("document-open").noDefaultShortcut();
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", tr("Delete Empty Annotations")).noDefaultShortcut();
	QAction *resize = makeAction("resizecanvas", tr("Resi&ze Canvas...")).noDefaultShortcut();
	QAction *canvasBackground = makeAction("canvas-background", tr("Set Session Background...")).noDefaultShortcut();
	QAction *setLocalBackground = makeAction("set-local-background", tr("Set Local Background...")).noDefaultShortcut();
	QAction *clearLocalBackground = makeAction("clear-local-background", tr("Clear Local Background")).noDefaultShortcut();
	QAction *brushSettings = makeAction("brushsettings", tr("&Brush Settings")).shortcut("F7");
	QAction *preferences = makeAction("preferences", tr("Prefere&nces")).noDefaultShortcut().menuRole(QAction::PreferencesRole);

	QAction *selectall = makeAction("selectall", tr("Select &All")).shortcut(QKeySequence::SelectAll);
	QAction *selectnone = makeAction("selectnone", tr("&Deselect"))
#if (defined(Q_OS_MACOS) || defined(Q_OS_WIN)) // Deselect is not defined on Mac and Win
		.shortcut("Shift+Ctrl+A")
#else
		.shortcut(QKeySequence::Deselect)
#endif
	;

	QAction *expandup = makeAction("expandup", tr("Expand &Up")).shortcut(CTRL_KEY | Qt::Key_J);
	QAction *expanddown = makeAction("expanddown", tr("Expand &Down")).shortcut(CTRL_KEY | Qt::Key_K);
	QAction *expandleft = makeAction("expandleft", tr("Expand &Left")).shortcut(CTRL_KEY | Qt::Key_H);
	QAction *expandright = makeAction("expandright", tr("Expand &Right")).shortcut(CTRL_KEY | Qt::Key_L);

	QAction *cleararea = makeAction("cleararea", tr("Delete")).shortcut(QKeySequence::Delete);
	QAction *fillfgarea = makeAction("fillfgarea", tr("Fill Selection")).shortcut(CTRL_KEY | Qt::Key_Comma);
	QAction *recolorarea = makeAction("recolorarea", tr("Recolor Selection")).shortcut(CTRL_KEY | Qt::SHIFT | Qt::Key_Comma);
	QAction *colorerasearea = makeAction("colorerasearea", tr("Color Erase Selection")).shortcut(Qt::SHIFT | Qt::Key_Delete);

	m_currentdoctools->addAction(copy);
	m_currentdoctools->addAction(copylayer);
	m_currentdoctools->addAction(deleteAnnotations);
	m_currentdoctools->addAction(selectall);
	m_currentdoctools->addAction(selectnone);

	m_undotools->addAction(undo);
	m_undotools->addAction(redo);

	m_putimagetools->addAction(cutlayer);
	m_putimagetools->addAction(paste);
	m_putimagetools->addAction(pasteCentered);
	m_putimagetools->addAction(pastefile);
	m_putimagetools->addAction(stamp);
	m_putimagetools->addAction(cleararea);
	m_putimagetools->addAction(fillfgarea);
	m_putimagetools->addAction(recolorarea);
	m_putimagetools->addAction(colorerasearea);

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
	connect(stamp, &QAction::triggered, m_doc, &Document::stamp);
	connect(pastefile, SIGNAL(triggered()), this, SLOT(pasteFile()));
	connect(selectall, &QAction::triggered, this, [this]() {
		QAction *selectRect = getAction("toolselectrect");
		if(!selectRect->isChecked())
			selectRect->trigger();
		m_doc->selectAll();
	});
	connect(selectnone, &QAction::triggered, m_doc, &Document::selectNone);
	connect(deleteAnnotations, &QAction::triggered, m_doc, &Document::removeEmptyAnnotations);
	connect(cleararea, &QAction::triggered, this, &MainWindow::clearOrDelete);
	connect(fillfgarea, &QAction::triggered, this, [this]() { m_doc->fillArea(m_dockToolSettings->foregroundColor(), DP_BLEND_MODE_NORMAL); });
	connect(recolorarea, &QAction::triggered, this, [this]() { m_doc->fillArea(m_dockToolSettings->foregroundColor(), DP_BLEND_MODE_RECOLOR); });
	connect(colorerasearea, &QAction::triggered, this, [this]() { m_doc->fillArea(m_dockToolSettings->foregroundColor(), DP_BLEND_MODE_COLOR_ERASE); });
	connect(resize, SIGNAL(triggered()), this, SLOT(resizeCanvas()));
	connect(canvasBackground, &QAction::triggered, this, &MainWindow::changeCanvasBackground);
	connect(setLocalBackground, &QAction::triggered, this, &MainWindow::changeLocalCanvasBackground);
	connect(clearLocalBackground, &QAction::triggered, this, &MainWindow::clearLocalCanvasBackground);
	connect(brushSettings, &QAction::triggered, this, &MainWindow::showBrushSettingsDialog);
	connect(preferences, SIGNAL(triggered()), this, SLOT(showSettings()));

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
	editmenu->addAction(stamp);
	editmenu->addSeparator();

	editmenu->addAction(selectall);
	editmenu->addAction(selectnone);
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
	editmenu->addAction(fillfgarea);
	editmenu->addAction(recolorarea);
	editmenu->addAction(colorerasearea);
	editmenu->addSeparator();
	editmenu->addAction(brushSettings);
	editmenu->addAction(preferences);

	QToolBar *edittools = new QToolBar(tr("Edit Tools"));
	edittools->setObjectName("edittoolsbar");
	toggletoolbarmenu->addAction(edittools->toggleViewAction());
	edittools->addAction(undo);
	edittools->addAction(redo);
	edittools->addAction(cutlayer);
	edittools->addAction(copylayer);
	edittools->addAction(paste);
	addToolBar(Qt::TopToolBarArea, edittools);

	//
	// View menu
	//
	QAction *layoutsAction = makeAction("layouts", tr("&Layouts..."));

	QAction *toolbartoggles = new QAction(tr("&Toolbars"), this);
	toolbartoggles->setMenu(toggletoolbarmenu);

	QAction *docktoggles = new QAction(tr("&Docks"), this);
	docktoggles->setMenu(toggledockmenu);

	QAction *toggleChat = makeAction("togglechat", tr("Chat")).shortcut("Alt+C").checked();


	QAction *zoomin = makeAction("zoomin", tr("Zoom &In")).icon("zoom-in").shortcut(QKeySequence::ZoomIn);
	QAction *zoomout = makeAction("zoomout", tr("Zoom &Out")).icon("zoom-out").shortcut(QKeySequence::ZoomOut);
	QAction *zoomorig = makeAction("zoomone", tr("&Normal Size")).icon("zoom-original").shortcut(QKeySequence("ctrl+0"));
	QAction *rotateorig = makeAction("rotatezero", tr("&Reset Rotation")).icon("transform-rotate").shortcut(QKeySequence("ctrl+r"));
	QAction *rotatecw = makeAction("rotatecw", tr("Rotate Canvas Clockwise")).shortcut(QKeySequence("shift+.")).icon("object-rotate-right");
	QAction *rotateccw = makeAction("rotateccw", tr("Rotate Canvas Counterclockwise")).shortcut(QKeySequence("shift+,")).icon("object-rotate-left");

	QAction *viewmirror = makeAction("viewmirror", tr("Mirror")).icon("object-flip-horizontal").shortcut("V").checkable();
	QAction *viewflip = makeAction("viewflip", tr("Flip")).icon("object-flip-vertical").shortcut("C").checkable();

	QAction *showannotations = makeAction("showannotations", tr("Show &Annotations")).noDefaultShortcut().checked().remembered();
	QAction *showusermarkers = makeAction("showusermarkers", tr("Show User &Pointers")).noDefaultShortcut().checked().remembered();
	QAction *showusernames = makeAction("showmarkernames", tr("Show Names")).noDefaultShortcut().checked().remembered();
	QAction *showuserlayers = makeAction("showmarkerlayers", tr("Show Layers")).noDefaultShortcut().checked().remembered();
	QAction *showuseravatars = makeAction("showmarkeravatars", tr("Show Avatars")).noDefaultShortcut().checked().remembered();
	QAction *showlasers = makeAction("showlasers", tr("Show La&ser Trails")).noDefaultShortcut().checked().remembered();
	QAction *showgrid = makeAction("showgrid", tr("Show Pixel &Grid")).noDefaultShortcut().checked().remembered();

#ifndef Q_OS_ANDROID
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
	connect(m_chatbox, &widgets::ChatBox::expandedChanged, toggleChat, &QAction::setChecked);
	connect(m_chatbox, &widgets::ChatBox::expandedChanged, m_statusChatButton, &QToolButton::hide);
	connect(m_chatbox, &widgets::ChatBox::expandPlease, toggleChat, &QAction::trigger);
	connect(toggleChat, &QAction::triggered, this, [this](bool show) {
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
	});

	connect(zoomin, &QAction::triggered, m_view, &widgets::CanvasView::zoomin);
	connect(zoomout, &QAction::triggered, m_view, &widgets::CanvasView::zoomout);
	connect(zoomorig, &QAction::triggered, this, [this]() { m_view->setZoom(100.0); });
	connect(rotateorig, &QAction::triggered, this, [this]() { m_view->setRotation(0); });
	connect(rotatecw, &QAction::triggered, this, [this]() { m_view->setRotation(m_view->rotation() + 5); });
	connect(rotateccw, &QAction::triggered, this, [this]() { m_view->setRotation(m_view->rotation() - 5); });
	connect(viewflip, SIGNAL(triggered(bool)), m_view, SLOT(setViewFlip(bool)));
	connect(viewmirror, SIGNAL(triggered(bool)), m_view, SLOT(setViewMirror(bool)));

#ifndef Q_OS_ANDROID
	connect(fullscreen, &QAction::triggered, this, &MainWindow::toggleFullscreen);
#endif

	connect(showannotations, &QAction::toggled, this, &MainWindow::setShowAnnotations);
	connect(showusermarkers, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserMarkers);
	connect(showusernames, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserNames);
	connect(showuserlayers, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserLayers);
	connect(showuseravatars, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserAvatars);
	connect(showlasers, &QAction::toggled, this, &MainWindow::setShowLaserTrails);
	connect(showgrid, &QAction::toggled, m_view, &widgets::CanvasView::setPixelGrid);

	m_viewstatus->setActions(viewflip, viewmirror, rotateorig, zoomorig);

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(layoutsAction);
	viewmenu->addAction(toolbartoggles);
	viewmenu->addAction(docktoggles);
	viewmenu->addAction(toggleChat);
	viewmenu->addSeparator();

	QMenu *zoommenu = viewmenu->addMenu(tr("&Zoom"));
	zoommenu->addAction(zoomin);
	zoommenu->addAction(zoomout);
	zoommenu->addAction(zoomorig);

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
	QAction *layerUncensor = makeAction("layerviewuncensor", tr("Show Censored Layers")).noDefaultShortcut().checkable().remembered();
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

	viewmenu->addAction(showannotations);

	viewmenu->addAction(showgrid);

#ifndef Q_OS_ANDROID
	viewmenu->addSeparator();
	viewmenu->addAction(fullscreen);
#endif

	//
	// Layer menu
	//
	QAction *layerAdd = makeAction("layeradd", tr("New Layer")).shortcut("Shift+Ctrl+Insert").icon("list-add");
	QAction *groupAdd = makeAction("groupadd", tr("New Group")).icon("folder-new").noDefaultShortcut();
	QAction *layerDupe = makeAction("layerdupe", tr("Duplicate Layer")).icon("edit-copy").noDefaultShortcut();
	QAction *layerMerge = makeAction("layermerge", tr("Merge Layer")).icon("arrow-down-double").noDefaultShortcut();
	QAction *layerProperties = makeAction("layerproperties", tr("Properties...")).icon("configure").noDefaultShortcut();
	QAction *layerDelete = makeAction("layerdelete", tr("Delete Layer")).icon("trash-empty").noDefaultShortcut();
	QAction *layerSetFillSource = makeAction("layersetfillsource", tr("Set as Fill Source")).icon("fill-color").noDefaultShortcut();

	QAction *layerUpAct = makeAction("layer-up", tr("Select Above")).shortcut("Shift+X");
	QAction *layerDownAct = makeAction("layer-down", tr("Select Below")).shortcut("Shift+Z");

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
	// Animation menu
	//
	QAction *showFlipbook = makeAction("showflipbook", tr("Flipbook")).statusTip(tr("Show animation preview window")).shortcut("Ctrl+F");
	QAction *frameCountSet = makeAction("frame-count-set", tr("Change Frame Count...")).icon("edit-rename").noDefaultShortcut();
	QAction *framerateSet = makeAction("framerate-set", tr("Change Frame Rate (FPS)...")).icon("edit-rename").noDefaultShortcut();
	QAction *keyFrameSetLayer = makeAction("key-frame-set-layer", tr("Set Key Frame to Current Layer")).icon("keyframe-add").shortcut("Ctrl+Shift+F");
	QAction *keyFrameSetEmpty = makeAction("key-frame-set-empty", tr("Set Blank Key Frame")).icon("keyframe-disable").shortcut("Ctrl+Shift+B");
	QAction *keyFrameCut = makeAction("key-frame-cut", tr("Cut Key Frame")).icon("edit-cut").noDefaultShortcut();
	QAction *keyFrameCopy = makeAction("key-frame-copy", tr("Copy Key Frame")).icon("edit-copy").noDefaultShortcut();
	QAction *keyFramePaste = makeAction("key-frame-paste", tr("Paste Key Frame")).icon("edit-paste").noDefaultShortcut();
	QAction *keyFrameProperties = makeAction("key-frame-properties", tr("Key Frame Properties...")).icon("configure").shortcut("Ctrl+Shift+P");
	QAction *keyFrameDelete = makeAction("key-frame-delete", tr("Delete Key Frame")).icon("keyframe-remove").shortcut("Ctrl+Shift+G");
	QAction *trackAdd = makeAction("track-add", tr("New Track")).icon("list-add").noDefaultShortcut();
	QAction *trackVisible = makeAction("track-visible", tr("Track Visible")).checkable().noDefaultShortcut();
	QAction *trackOnionSkin = makeAction("track-onion-skin", tr("Track Onion Skin")).checkable().shortcut("Ctrl+Shift+O");
	QAction *trackDuplicate = makeAction("track-duplicate", tr("Duplicate Track")).icon("edit-copy").noDefaultShortcut();
	QAction *trackRetitle = makeAction("track-retitle", tr("Rename Track")).icon("edit-rename").noDefaultShortcut();
	QAction *trackDelete = makeAction("track-delete", tr("Delete Track")).icon("trash-empty").noDefaultShortcut();
	QAction *frameNext = makeAction("frame-next", tr("Next Frame")).icon("keyframe-next").shortcut("Ctrl+Shift+L");
	QAction *framePrev = makeAction("frame-prev", tr("Previous Frame")).icon("keyframe-previous").shortcut("Ctrl+Shift+H");
	QAction *trackAbove = makeAction("track-above", tr("Track Above")).icon("arrow-up").shortcut("Ctrl+Shift+K");
	QAction *trackBelow = makeAction("track-below", tr("Track Below")).icon("arrow-down").shortcut("Ctrl+Shift+J");

	QAction *keyFrameCreateLayer = makeAction("key-frame-create-layer", tr("Create Layer on Current Key Frame")).icon("keyframe-add").shortcut("Ctrl+Shift+R");
	QAction *keyFrameCreateLayerNext = makeAction("key-frame-create-layer-next", tr("Create Layer on Next Key Frame")).icon("keyframe-next").shortcut("Ctrl+Shift+T");
	QAction *keyFrameCreateLayerPrev = makeAction("key-frame-create-layer-prev", tr("Create Layer on Previous Key Frame")).icon("keyframe-previous").shortcut("Ctrl+Shift+E");
	QAction *keyFrameCreateGroup = makeAction("key-frame-create-group", tr("Create Group on Current Key Frame")).icon("keyframe-add").noDefaultShortcut();
	QAction *keyFrameCreateGroupNext = makeAction("key-frame-create-group-next", tr("Create Group on Next Key Frame")).icon("keyframe-next").noDefaultShortcut();
	QAction *keyFrameCreateGroupPrev = makeAction("key-frame-create-group-prev", tr("Create Group on Previous Key Frame")).icon("keyframe-previous").noDefaultShortcut();

	QActionGroup *layerKeyFrameGroup = new QActionGroup{this};
	layerKeyFrameGroup->addAction(keyFrameCreateLayer);
	layerKeyFrameGroup->addAction(keyFrameCreateLayerNext);
	layerKeyFrameGroup->addAction(keyFrameCreateLayerPrev);
	layerKeyFrameGroup->addAction(keyFrameCreateGroup);
	layerKeyFrameGroup->addAction(keyFrameCreateGroupNext);
	layerKeyFrameGroup->addAction(keyFrameCreateGroupPrev);

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
	animationMenu->addAction(trackAbove);
	animationMenu->addAction(trackBelow);

	m_currentdoctools->addAction(showFlipbook);
	m_dockLayers->setLayerEditActions({layerAdd, groupAdd, layerDupe, layerMerge, layerProperties, layerDelete, layerSetFillSource, keyFrameSetLayer, keyFrameCreateLayer, keyFrameCreateLayerNext, keyFrameCreateLayerPrev, keyFrameCreateGroup, keyFrameCreateGroupNext, keyFrameCreateGroupPrev, layerKeyFrameGroup});
	m_dockTimeline->setActions({keyFrameSetLayer, keyFrameSetEmpty, keyFrameCreateLayer, keyFrameCreateLayerNext, keyFrameCreateLayerPrev, keyFrameCreateGroup, keyFrameCreateGroupNext, keyFrameCreateGroupPrev, keyFrameCut, keyFrameCopy, keyFramePaste, keyFrameProperties, keyFrameDelete, trackAdd, trackVisible, trackOnionSkin, trackDuplicate, trackRetitle, trackDelete, frameCountSet, framerateSet, frameNext, framePrev, trackAbove, trackBelow, animationLayerMenu, animationGroupMenu}, layerViewNormal, layerViewCurrentFrame);

	connect(showFlipbook, &QAction::triggered, this, &MainWindow::showFlipbook);

	//
	// Session menu
	//
	QAction *host = makeAction("hostsession", tr("&Host...")).statusTip(tr("Share your drawingboard with others"));
	QAction *join = makeAction("joinsession", tr("&Join...")).statusTip(tr("Join another user's drawing session"));
	QAction *logout = makeAction("leavesession", tr("&Leave")).statusTip(tr("Leave this drawing session")).disabled();

	QAction *serverlog = makeAction("viewserverlog", tr("Event Log")).noDefaultShortcut();
	QAction *sessionSettings = makeAction("sessionsettings", tr("Settings...")).noDefaultShortcut().menuRole(QAction::NoRole).disabled();
	QAction *sessionUndoDepthLimit = makeAction("sessionundodepthlimit").disabled();

	QAction *gainop = makeAction("gainop", tr("Become Operator...")).disabled();
	QAction *resetsession = makeAction("resetsession", tr("&Reset..."));
	QAction *terminatesession = makeAction("terminatesession", tr("Terminate"));
	QAction *reportabuse = makeAction("reportabuse", tr("Report...")).disabled();

	QAction *locksession = makeAction("locksession", tr("Lock Everything")).statusTip(tr("Prevent changes to the drawing board")).shortcut("F12").checkable();

	m_admintools->addAction(locksession);
	m_modtools->addAction(terminatesession);
	m_admintools->setEnabled(false);

	connect(host, &QAction::triggered, this, &MainWindow::host);
	connect(join, SIGNAL(triggered()), this, SLOT(join()));
	connect(logout, &QAction::triggered, this, &MainWindow::leave);
	connect(sessionSettings, &QAction::triggered, m_sessionSettings, [this](){
		utils::showWindow(m_sessionSettings);
	});
	connect(sessionUndoDepthLimit, &QAction::triggered, this, &MainWindow::changeUndoDepthLimit);
	connect(serverlog, &QAction::triggered, m_serverLogDialog, [this](){
		utils::showWindow(m_serverLogDialog);
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
	sessionmenu->addAction(join);
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

	//
	// Tools menu and toolbar
	//
	QAction *freehandtool = makeAction("toolbrush", tr("Freehand")).icon("draw-brush").statusTip(tr("Freehand brush tool")).shortcut("B").checkable();
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
	QAction *zoomtool = makeAction("toolzoom", tr("Zoom")).icon("zoom-select").statusTip(tr("Zoom the canvas view")).shortcut("Z").checkable();
	QAction *inspectortool = makeAction("toolinspector", tr("Inspector")).icon("help-whatsthis").statusTip(tr("Find out who did it")).shortcut("Ctrl+I").checkable();

	m_drawingtools->addAction(freehandtool);
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
	m_drawingtools->addAction(zoomtool);
	m_drawingtools->addAction(inspectortool);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(m_drawingtools->actions());

	QMenu *toolshortcuts = toolsmenu->addMenu(tr("&Shortcuts"));

	QMenu *devtoolsmenu = toolsmenu->addMenu(tr("Developer Tools"));
	QAction *tableteventlog = makeAction("tableteventlog");
	QAction *profile = makeAction("profile");
	QAction *artificialLag = makeAction("artificiallag", tr("Set Artificial Lag..."));
	QAction *artificialDisconnect = makeAction("artificialdisconnect", tr("Artifical Disconnect..."));
	QAction *debugDump = makeAction("debugdump", tr("Record Debug Dumps")).checkable();
	QAction *openDebugDump = makeAction("opendebugdump", tr("Open Debug Dump..."));
	devtoolsmenu->addAction(tableteventlog);
	devtoolsmenu->addAction(profile);
	devtoolsmenu->addAction(artificialLag);
	devtoolsmenu->addAction(artificialDisconnect);
	devtoolsmenu->addAction(debugDump);
	devtoolsmenu->addAction(openDebugDump);
	connect(devtoolsmenu, &QMenu::aboutToShow, this, &MainWindow::updateDevToolsActions);
	connect(tableteventlog, &QAction::triggered, this, &MainWindow::toggleTabletEventLog);
	connect(profile, &QAction::triggered, this, &MainWindow::toggleProfile);
	connect(artificialLag, &QAction::triggered, this, &MainWindow::setArtificialLag);
	connect(artificialDisconnect, &QAction::triggered, this, &MainWindow::setArtificialDisconnect);
	connect(debugDump, &QAction::triggered, this, &MainWindow::toggleDebugDump);
	connect(openDebugDump, &QAction::triggered, this, &MainWindow::openDebugDump);

	QAction *currentEraseMode = makeAction("currenterasemode", tr("Toggle Eraser Mode")).shortcut("Ctrl+E");
	QAction *currentRecolorMode = makeAction("currentrecolormode", tr("Toggle Recolor Mode")).shortcut("Shift+E");
	QAction *swapcolors = makeAction("swapcolors", tr("Swap Last Colors")).shortcut("X");
	QAction *smallerbrush = makeAction("ensmallenbrush", tr("&Decrease Brush Size")).shortcut(Qt::Key_BracketLeft);
	QAction *biggerbrush = makeAction("embiggenbrush", tr("&Increase Brush Size")).shortcut(Qt::Key_BracketRight);
	QAction *reloadPreset = makeAction("reloadpreset", tr("&Reload Last Brush Preset")).shortcut("Shift+P");

	smallerbrush->setAutoRepeat(true);
	biggerbrush->setAutoRepeat(true);

	connect(currentEraseMode, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::toggleEraserMode);
	connect(currentRecolorMode, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::toggleRecolorMode);
	connect(swapcolors, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::swapLastUsedColors);

	connect(smallerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(false); });
	connect(biggerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(true); });
	connect(reloadPreset, &QAction::triggered, m_dockBrushPalette, &docks::BrushPalette::reloadPreset);

	toolshortcuts->addAction(currentEraseMode);
	toolshortcuts->addAction(currentRecolorMode);
	toolshortcuts->addAction(swapcolors);
	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);
	toolshortcuts->addAction(reloadPreset);

	QToolBar *drawtools = new QToolBar(tr("Drawing tools"));
	drawtools->setObjectName("drawtoolsbar");
	toggletoolbarmenu->addAction(drawtools->toggleViewAction());

	for(QAction *dt : m_drawingtools->actions()) {
		// Add a separator before color picker to separate brushes from non-destructive tools
		if(dt == pickertool)
			drawtools->addSeparator();
		drawtools->addAction(dt);
	}

	addToolBar(Qt::TopToolBarArea, drawtools);

	//
	// Window menu (Mac only)
	//
#ifdef Q_OS_MACOS
	menuBar()->addMenu(MacMenu::instance()->windowMenu());
#endif

	//
	// Help menu
	//
	QAction *homepage = makeAction("dphomepage", tr("&Homepage")).statusTip(cmake_config::website());
	QAction *tablettester = makeAction("tablettester", tr("Tablet Tester"));
	QAction *showlogfile = makeAction("showlogfile", tr("Log File"));
	QAction *about = makeAction("dpabout", tr("&About Drawpile")).menuRole(QAction::AboutRole);
	QAction *aboutqt = makeAction("aboutqt", tr("About &Qt")).menuRole(QAction::AboutQtRole);
#ifdef ENABLE_VERSION_CHECK
	QAction *versioncheck = makeAction("versioncheck", tr("Check For Updates"));
#endif

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);

#ifdef ENABLE_VERSION_CHECK
	connect(versioncheck, &QAction::triggered, this, [this]() {
		auto *dlg = new dialogs::VersionCheckDialog(this);
		utils::showWindow(dlg);
		dlg->queryNewVersions();
	});
#endif

	connect(tablettester, &QAction::triggered, []() {
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
		utils::showWindow(ttd);
		ttd->raise();
	});

	connect(showlogfile, &QAction::triggered, [this] {
		QString logFilePath = utils::logFilePath();
		QFile logFile{logFilePath};
		if(!logFile.exists()) {
			QMessageBox::warning(
				this, tr("Missing Log File"),
				tr("Log file doesn't exist, do you need to enable logging in the preferences?"));
			return;
		}

#ifdef Q_OS_ANDROID
		QString filename = FileWrangler{this}.getSaveLogFilePath();
		if(!filename.isEmpty()) {
			if(!logFile.open(QIODevice::ReadOnly)) {
				QMessageBox::warning(
					this, tr("Error Saving Log File"),
					tr("Could not read source file: %1").arg(logFile.errorString()));
				return;
			}

			QFile targetFile{filename};
			if(!targetFile.open(QIODevice::WriteOnly)) {
				QMessageBox::warning(
					this, tr("Error Saving Log File"),
					tr("Could not open target file: %1").arg(targetFile.errorString()));
				return;
			}

			targetFile.write(logFile.readAll());
		}
#else
		QDesktopServices::openUrl(QUrl::fromLocalFile(utils::logFilePath()));
#endif
	});

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addAction(tablettester);
	helpmenu->addAction(showlogfile);
#ifndef Q_OS_MACOS
	// Qt shunts the About menus into the Application menu on macOS, so this
	// would cause two separators to be placed instead of one
	helpmenu->addSeparator();
#endif
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);
	helpmenu->addSeparator();
#ifdef ENABLE_VERSION_CHECK
	helpmenu->addAction(versioncheck);
#endif

	// Brush slot shortcuts

	m_brushSlots = new QActionGroup(this);
	for(int i=0;i<6;++i) {
		QAction *q = new QAction(QString("Brush slot #%1").arg(i+1), this);
		q->setAutoRepeat(false);
		q->setObjectName(QString("quicktoolslot-%1").arg(i));
		q->setShortcut(QKeySequence(QString::number(i+1)));
		q->setProperty("toolslotidx", i);
		CustomShortcutModel::registerCustomizableAction(q->objectName(), q->text(), q->shortcut());
		m_brushSlots->addAction(q);
		addAction(q);
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
}

void MainWindow::createDocks()
{
	Q_ASSERT(m_doc);
	Q_ASSERT(m_view);
	Q_ASSERT(m_canvasscene);

	setDockNestingEnabled(true);

	// Create tool settings
	m_dockToolSettings = new docks::ToolSettings(m_doc->toolCtrl(), this);
	m_dockToolSettings->setObjectName("ToolSettings");
	m_dockToolSettings->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::LeftDockWidgetArea, m_dockToolSettings);
	m_dockToolSettings->selectionSettings()->setView(m_view);
	m_dockToolSettings->annotationSettings()->setScene(m_canvasscene);

	// Create brush palette
	m_dockBrushPalette = new docks::BrushPalette(this);
	m_dockBrushPalette->setObjectName("BrushPalette");
	m_dockBrushPalette->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::LeftDockWidgetArea, m_dockBrushPalette);

	m_dockBrushPalette->connectBrushSettings(
		m_dockToolSettings->brushSettings());

	// Create color docks
	m_dockColorSpinner = new docks::ColorSpinnerDock(tr("Color Wheel"), this);
	m_dockColorSpinner->setObjectName("colorspinnerdock");
	m_dockColorSpinner->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSpinner);

	m_dockColorPalette = new docks::ColorPaletteDock(tr("Palette"), this);
	m_dockColorPalette->setObjectName("colorpalettedock");
	m_dockColorPalette->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorPalette);

	m_dockColorSliders = new docks::ColorSliderDock(tr("Color Sliders"), this);
	m_dockColorSliders->setObjectName("colorsliderdock");
	m_dockColorSliders->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSliders);

	tabifyDockWidget(m_dockColorPalette, m_dockColorSliders);
	tabifyDockWidget(m_dockColorSliders, m_dockColorSpinner);

	// Create layer list
	m_dockLayers = new docks::LayerList(this);
	m_dockLayers->setObjectName("LayerList");
	m_dockLayers->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::RightDockWidgetArea, m_dockLayers);

	// Create navigator
	m_dockNavigator = new docks::Navigator(this);
	m_dockNavigator->setObjectName("navigatordock");
	m_dockNavigator->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::RightDockWidgetArea, m_dockNavigator);
	m_dockNavigator->hide(); // hidden by default

	// Create onion skin settings
	m_dockOnionSkins = new docks::OnionSkinsDock(tr("Onion Skins"), this);
	m_dockOnionSkins->setObjectName("onionskins");
	m_dockOnionSkins->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::TopDockWidgetArea, m_dockOnionSkins);
	m_dockOnionSkins->hide(); // hidden by default

	// Create timeline
	m_dockTimeline = new docks::Timeline(this);
	m_dockTimeline->setObjectName("Timeline");
	m_dockTimeline->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::TopDockWidgetArea, m_dockTimeline);
	m_dockTimeline->hide(); // hidden by default

	tabifyDockWidget(m_dockOnionSkins, m_dockTimeline);
}
