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

#include <QApplication>
#include <QActionGroup>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QFileDialog>
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
#include <QSplitter>
#include <QClipboard>
#include <QFile>
#include <QWindow>
#include <QVBoxLayout>
#include <QTimer>
#include <QTextEdit>
#include <QThreadPool>

#include <QtColorWidgets/ColorDialog>

#ifdef Q_OS_OSX
#define CTRL_KEY "Meta"
#include "widgets/macmenu.h"
#else
#define CTRL_KEY "Ctrl"
#endif

#include "config.h"
#include "mainwindow.h"
#include "document.h"
#include "main.h"

#include "canvas/canvasmodel.h"
#include "scene/canvasview.h"
#include "scene/canvasscene.h"
#include "scene/selectionitem.h"
#include "canvas/userlist.h"
#include "canvas/paintengine.h"
#include "canvas/documentmetadata.h"

#include "utils/recentfiles.h"
#include "../libshared/util/whatismyip.h"
#include "utils/icon.h"
#include "utils/images.h"
#include "../libshared/util/networkaccess.h"
#include "../libshared/util/paths.h"
#include "utils/shortcutdetector.h"
#include "utils/customshortcutmodel.h"
#include "utils/logging.h"
#include "utils/actionbuilder.h"
#include "utils/hotbordereventfilter.h"

#include "widgets/viewstatus.h"
#include "widgets/netstatus.h"
#include "chat/chatbox.h"

#include "docks/toolsettingsdock.h"
#include "docks/brushpalettedock.h"
#include "docks/navigator.h"
#include "docks/colorpalette.h"
#include "docks/colorspinner.h"
#include "docks/colorsliders.h"
#include "docks/layerlistdock.h"
#include "docks/onionskins.h"
#include "docks/timeline.h"

#include "net/client.h"
#include "net/login.h"
#include "canvas/layerlist.h"
#include "parentalcontrols/parentalcontrols.h"

#include "tools/toolcontroller.h"
#include "toolwidgets/brushsettings.h"
#include "toolwidgets/colorpickersettings.h"
#include "toolwidgets/selectionsettings.h"
#include "toolwidgets/annotationsettings.h"
#include "toolwidgets/lasersettings.h"
#include "toolwidgets/zoomsettings.h"
#include "toolwidgets/inspectorsettings.h"

#include "export/animationsaverrunnable.h"
#include "../libshared/record/reader.h"
#include "drawdance/perf.h"

#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/logindialog.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/resizedialog.h"
#include "dialogs/playbackdialog.h"
#include "dialogs/flipbook.h"
#include "dialogs/resetdialog.h"
#include "dialogs/sessionsettings.h"
#include "dialogs/serverlogdialog.h"
#include "dialogs/tablettester.h"
#include "dialogs/abusereport.h"
#include "dialogs/versioncheckdialog.h"

#if defined(Q_OS_WIN) && defined(KIS_TABLET)
#include "bundled/kis_tablet/kis_tablet_support_win.h"
#endif

static QString getLastPath() { return QSettings().value("window/lastpath").toString(); }

static void setLastPath(const QString &lastpath) { QSettings().setValue("window/lastpath", lastpath); }

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
	  m_lastToolBeforePaste(-1),
	  m_fullscreenOldMaximized(false),
	  m_tempToolSwitchShortcut(nullptr),
	  m_doc(nullptr),
	  m_exitAfterSave(false)
{
	// The document (initially empty)
	m_doc = new Document(this);

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

#ifdef Q_OS_MAC
	// The "native" style status bar appears slightly glitchy.
	// This makes it look better.
	if(icon::isDark(palette().color(QPalette::Window)))
		m_viewStatusBar->setStyleSheet("QStatusBar { background: #323232 }");
	else
		m_viewStatusBar->setStyleSheet("QStatusBar { background: #ececec }");
#endif

	// Create status indicator widgets
	m_viewstatus = new widgets::ViewStatus(this);

	m_netstatus = new widgets::NetStatus(this);
	m_lockstatus = new QLabel(this);
	m_lockstatus->setFixedSize(QSize(16, 16));

	// Statusbar chat button: this is normally hidden and only shown
	// when there are unread chat messages.
	m_statusChatButton = new QToolButton(this);
	m_statusChatButton->setAutoRaise(true);
	m_statusChatButton->setIcon(icon::fromTheme("drawpile_chat"));
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
	connect(m_doc, &Document::dirtyCanvas, this, &MainWindow::setWindowModified);
	connect(m_doc, &Document::sessionTitleChanged, this, &MainWindow::updateTitle);
	connect(m_doc, &Document::currentFilenameChanged, this, &MainWindow::updateTitle);
	connect(m_doc, &Document::recorderStateChanged, this, &MainWindow::setRecorderStatus);

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

	connect(static_cast<tools::LaserPointerSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::LASERPOINTER)), &tools::LaserPointerSettings::pointerTrackingToggled,
		m_view, &widgets::CanvasView::setPointerTracking);
	connect(static_cast<tools::ZoomSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::ZOOM)), &tools::ZoomSettings::resetZoom,
		this, [this]() { m_view->setZoom(100.0); });
	connect(static_cast<tools::ZoomSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::ZOOM)), &tools::ZoomSettings::fitToWindow,
		m_view, &widgets::CanvasView::zoomToFit);

	tools::BrushSettings *brushSettings = static_cast<tools::BrushSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::FREEHAND));
	connect(brushSettings, &tools::BrushSettings::pressureMappingChanged, m_view, &widgets::CanvasView::setPressureMapping);

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

	// Tool controller <-> UI connections
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged, m_canvasscene, &drawingboard::CanvasScene::setActiveAnnotation);
	connect(m_doc->toolCtrl(), &tools::ToolController::colorUsed, m_dockToolSettings, &docks::ToolSettings::addLastUsedColor);
	connect(m_doc->toolCtrl(), &tools::ToolController::zoomRequested, m_view, &widgets::CanvasView::zoomTo);

	connect(m_canvasscene, &drawingboard::CanvasScene::annotationDeleted, this, [this](int id) {
		if(m_doc->toolCtrl()->activeAnnotation() == id)
			m_doc->toolCtrl()->setActiveAnnotation(0);
	});

	connect(brushSettings, &tools::BrushSettings::smoothingChanged, m_doc->toolCtrl(), &tools::ToolController::setSmoothing);
	m_doc->toolCtrl()->setSmoothing(brushSettings->getSmoothing());
	connect(m_doc->toolCtrl(), &tools::ToolController::toolCursorChanged, m_view, &widgets::CanvasView::setToolCursor);
	m_view->setToolCursor(m_doc->toolCtrl()->activeToolCursor());

	connect(m_view, &widgets::CanvasView::penDown, m_doc->toolCtrl(), &tools::ToolController::startDrawing);
	connect(m_view, &widgets::CanvasView::penMove, m_doc->toolCtrl(), &tools::ToolController::continueDrawing);
	connect(m_view, &widgets::CanvasView::penHover, m_doc->toolCtrl(), &tools::ToolController::hoverDrawing);
	connect(m_view, &widgets::CanvasView::penUp, m_doc->toolCtrl(), &tools::ToolController::endDrawing);
	connect(m_view, &widgets::CanvasView::quickAdjust, m_dockToolSettings, &docks::ToolSettings::quickAdjustCurrent1);

	connect(m_dockLayers, &docks::LayerList::layerSelected, m_doc->toolCtrl(), &tools::ToolController::setActiveLayer);
	connect(m_dockLayers, &docks::LayerList::layerSelected, m_dockTimeline, &docks::Timeline::setCurrentLayer);
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged,
			static_cast<tools::AnnotationSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::ANNOTATION)), &tools::AnnotationSettings::setSelectionId);

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
	connect(m_doc, &Document::sessionNsfmChanged, this, &MainWindow::onNsfmChanged);

	connect(m_doc, &Document::serverConnected, m_netstatus, &widgets::NetStatus::connectingToHost);
	connect(m_doc->client(), &net::Client::serverDisconnecting, m_netstatus, &widgets::NetStatus::hostDisconnecting);
	connect(m_doc, &Document::serverDisconnected, m_netstatus, &widgets::NetStatus::hostDisconnected);
	connect(m_doc, &Document::sessionRoomcodeChanged, m_netstatus, &widgets::NetStatus::setRoomcode);

	connect(m_doc->client(), SIGNAL(bytesReceived(int)), m_netstatus, SLOT(bytesReceived(int)));
	connect(m_doc->client(), &net::Client::bytesSent, m_netstatus, &widgets::NetStatus::bytesSent);
	connect(m_doc->client(), &net::Client::lagMeasured, m_netstatus, &widgets::NetStatus::lagMeasured);
	connect(m_doc->client(), &net::Client::youWereKicked, m_netstatus, &widgets::NetStatus::kicked);

	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(loadShortcuts()));
	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));
	connect(qApp, SIGNAL(settingsChanged()), m_view, SLOT(updateShortcuts()));

	updateSettings();

	// Create actions and menus
	setupActions();
	setDrawingToolsEnabled(false);

	// Restore settings
	readSettings(restoreWindowPosition);

	// Set status indicators
	updateLockWidget();
	setRecorderStatus(false);

#ifdef Q_OS_MAC
	MacMenu::instance()->addWindow(this);

#else
	// OSX provides this feature itself
	HotBorderEventFilter *hbfilter = new HotBorderEventFilter(this);
	m_view->installEventFilter(hbfilter);
	for(QObject *c : children()) {
		QToolBar *tb = dynamic_cast<QToolBar*>(c);
		if(tb)
			tb->installEventFilter(hbfilter);
	}

	connect(hbfilter, &HotBorderEventFilter::hotBorder, this, &MainWindow::hotBorderMenubar);
#endif

	// Show self
	updateTitle();
	show();

	setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::TopRightCorner, Qt::RightDockWidgetArea);
	setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
	setCorner(Qt::TopLeftCorner, Qt::LeftDockWidgetArea);

	static bool warningShown;
	if(!warningShown) {
		QMessageBox::warning(this, tr("Dancepile Test Build"), tr(
			"<p><strong>This is a development test build.</strong> Do <strong>not</strong> report problems in the official Drawpile issue tracker. Tell me about them on Discord or something instead.</p>"
			"<p>This has not been fully tested. There are probably still bugs. The application icon is a ferret. Save often.</p>"
#ifdef DRAWDANCE_HISTORY_DUMP
			"<p><strong>This build will automatically record debug dumps.</strong> They should appear in the application directory and will have a \".drawdancedump\" extension. You can delete them if you don't run into any issues. If you <em>do</em> encounter a problem, keep them around, they might help figure out why it happened.</p>"
#endif
			));
	}
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MAC
	MacMenu::instance()->removeWindow(this);
#endif

	// Clear this out first so there will be no weird signals emitted
	// while the document is being torn down.
	m_view->setScene(nullptr);
	delete m_canvasscene;

	// Make sure all child dialogs are closed
	QObjectList lst = children();
	for(QObject *obj : lst) {
		QDialog *child = qobject_cast<QDialog*>(obj);
		delete child;
	}
}

void MainWindow::onCanvasChanged(canvas::CanvasModel *canvas)
{
	m_canvasscene->initCanvas(canvas);

	connect(canvas->aclState(), &canvas::AclState::localOpChanged, this, &MainWindow::onOperatorModeChange);
	connect(canvas->aclState(), &canvas::AclState::localLockChanged, this, &MainWindow::updateLockWidget);
	connect(canvas->aclState(), &canvas::AclState::featureAccessChanged, this, &MainWindow::onFeatureAccessChange);

	connect(canvas, &canvas::CanvasModel::chatMessageReceived, this, [this]() {
		// Show a "new message" indicator when the chatbox is collapsed
		const auto sizes = m_splitter->sizes();
		if(sizes.length() > 1 && sizes.at(1)==0)
			m_statusChatButton->show();
	});

	connect(canvas, &canvas::CanvasModel::layerAutoselectRequest, m_dockLayers, &docks::LayerList::selectLayer);
	connect(canvas, &canvas::CanvasModel::colorPicked, m_dockToolSettings, &docks::ToolSettings::setForegroundColor);
	connect(canvas, &canvas::CanvasModel::colorPicked, static_cast<tools::ColorPickerSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::PICKER)), &tools::ColorPickerSettings::addColor);
	connect(canvas, &canvas::CanvasModel::canvasInspected, static_cast<tools::InspectorSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::INSPECTOR)), &tools::InspectorSettings::onCanvasInspected);
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
	m_dockTimeline->setTimeline(canvas->timeline());

	m_dockTimeline->setFps(canvas->metadata()->framerate());
	m_dockTimeline->setUseTimeline(canvas->metadata()->useTimeline());
	connect(canvas->metadata(), &canvas::DocumentMetadata::framerateChanged, m_dockTimeline, &docks::Timeline::setFps);
	connect(canvas->metadata(), &canvas::DocumentMetadata::useTimelineChanged, m_dockTimeline, &docks::Timeline::setUseTimeline);

	connect(m_dockTimeline, &docks::Timeline::currentFrameChanged, canvas->paintEngine(), &canvas::PaintEngine::setViewFrame);
	connect(m_dockTimeline, &docks::Timeline::layerSelectRequested, m_dockLayers, &docks::LayerList::selectLayer);

	connect(m_dockOnionSkins, &docks::OnionSkinsDock::onionSkinsChanged, canvas->paintEngine(), &canvas::PaintEngine::setOnionSkins);
	m_dockOnionSkins->triggerUpdate();

	static_cast<tools::InspectorSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::INSPECTOR))->setUserList(m_canvasscene->model()->userlist());

	// Make sure the UI matches the default feature access level
	m_currentdoctools->setEnabled(true);
	setDrawingToolsEnabled(true);
	for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
		DP_Feature f = DP_Feature(i);
		onFeatureAccessChange(f, m_doc->canvas()->aclState()->canUseFeature(f));
	}
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
 *
 * @retval false if a new window needs to be created
 */
bool MainWindow::canReplace() const {
	return !(m_doc->isDirty() || m_doc->client()->isConnected() || m_doc->isRecording() || m_playbackDialog);
}

/**
 * Get either a new MainWindow or this one if replacable
 */
MainWindow *MainWindow::replaceableWindow()
{
	if(!canReplace()) {
		if(windowState().testFlag(Qt::WindowFullScreen))
			toggleFullscreen();
		getAction("hidedocks")->setChecked(false);
		writeSettings();
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
#ifdef Q_OS_MAC
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
		name = info.baseName();
	}

	if(m_doc->sessionTitle().isEmpty())
		setWindowTitle(QStringLiteral("%1[*]").arg(name));
	else
		setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, m_doc->sessionTitle()));

#ifdef Q_OS_MAC
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
void MainWindow::loadShortcuts()
{
	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");

	static const QRegularExpression shortcutAmpersand { "&([^&])" };

	disconnect(m_textCopyConnection);
	const QKeySequence standardCopyShortcut { QKeySequence::Copy };

	QList<QAction*> actions = findChildren<QAction*>();
	for(QAction *a : actions) {
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

void MainWindow::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/input");

	const bool enable = cfg.value("tabletevents", true).toBool();
	const bool eraser = cfg.value("tableteraser", true).toBool();

	m_view->setTabletEnabled(enable);

#if defined(Q_OS_WIN) && defined(KIS_TABLET)
	KisTabletSupportWin::enableRelativePenModeHack(cfg.value("relativepenhack", false).toBool());
#endif

	// Handle eraser event
	if(eraser)
		connect(qApp, SIGNAL(eraserNear(bool)), m_dockToolSettings, SLOT(eraserNear(bool)), Qt::UniqueConnection);
	else
		disconnect(qApp, SIGNAL(eraserNear(bool)), m_dockToolSettings, SLOT(eraserNear(bool)));

	// not really tablet related, but close enough
	m_view->setTouchGestures(
		cfg.value("touchscroll", true).toBool(),
		cfg.value("touchpinch", true).toBool(),
		cfg.value("touchtwist", true).toBool()
	);
	cfg.endGroup();

	cfg.beginGroup("settings");
	m_view->setBrushCursorStyle(cfg.value("brushcursor").toInt(), cfg.value("brushoutlinewidth").toReal());
	static_cast<tools::BrushSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::FREEHAND))->setShareBrushSlotColor(cfg.value("sharebrushslotcolor", false).toBool());
	cfg.endGroup();
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
		getAction("layerviewonionskin"),
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
	bool enableOnionSkins;
	QAction *action;
	if((action = getAction("layerviewcurrentlayer"))->isChecked()) {
		mode = DP_VIEW_MODE_LAYER;
		enableOnionSkins = false;
	} else if((action = getAction("layerviewcurrentframe"))->isChecked()) {
		mode = DP_VIEW_MODE_FRAME;
		enableOnionSkins = false;
	} else if((action = getAction("layerviewonionskin"))->isChecked()) {
		mode = DP_VIEW_MODE_FRAME;
		enableOnionSkins = true;
	} else {
		action = getAction("layerviewnormal");
		mode = DP_VIEW_MODE_NORMAL;
		enableOnionSkins = false;
	}
	m_lastLayerViewMode = action;

	m_doc->canvas()->paintEngine()->setViewMode(mode, censor, enableOnionSkins);
	updateLockWidget();
}

/**
 * Read and apply mainwindow related settings.
 */
void MainWindow::readSettings(bool windowpos)
{
	QSettings cfg;
	cfg.beginGroup("window");

	// Restore previously used window size and position
	resize(cfg.value("size",QSize(800,600)).toSize());

	if(windowpos && cfg.contains("pos")) {
		const QPoint pos = cfg.value("pos").toPoint();
		if(qApp->primaryScreen()->availableGeometry().contains(pos))
			move(pos);
	}

	bool maximize = cfg.value("maximized", false).toBool();
	if(maximize)
		setWindowState(Qt::WindowMaximized);

	// Restore dock, toolbar and view states
	if(cfg.contains("state")) {
		restoreState(cfg.value("state").toByteArray());
	}
	if(cfg.contains("viewstate")) {
		m_splitter->restoreState(cfg.value("viewstate").toByteArray());
	}

	// Restore remembered actions
	cfg.beginGroup("actions");
	for(QAction *act : actions()) {
		if(act->isCheckable() && act->property("remembered").toBool()) {
			act->setChecked(cfg.value(act->objectName(), act->property("defaultValue")).toBool());
		}
	}
	cfg.endGroup();
	cfg.endGroup();

	// Restore tool settings
	m_dockToolSettings->readSettings();

	// Customize shortcuts
	loadShortcuts();

	// Restore recent files
	RecentFiles::initMenu(m_recentMenu);
}

/**
 * Write out settings
 */
void MainWindow::writeSettings()
{
	QSettings cfg;
	cfg.beginGroup("window");

	cfg.setValue("pos", normalGeometry().topLeft());
	cfg.setValue("size", normalGeometry().size());

	cfg.setValue("maximized", isMaximized());
	cfg.setValue("state", saveState());
	cfg.setValue("viewstate", m_splitter->saveState());

	// Save all remembered actions
	cfg.beginGroup("actions");
	for(const QAction *act : actions()) {
		if(act->isCheckable() && act->property("remembered").toBool())
			cfg.setValue(act->objectName(), act->isChecked());
	}
	cfg.endGroup();
	cfg.endGroup();

	m_dockToolSettings->saveSettings();
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
	if(event->type() == QEvent::StatusTip) {
		m_viewStatusBar->showMessage(static_cast<QStatusTipEvent*>(event)->tip());
		return true;
	} else {
		// Monitor key-up events to switch back from temporary tools/tool slots.
		// A short tap of the tool switch shortcut switches the tool permanently as usual,
		// but when holding it down, the tool is activated just temporarily. The
		// previous tool be switched back automatically when the shortcut key is released.
		// Note: for simplicity, we only support tools with single key shortcuts.
		if(event->type() == QEvent::KeyRelease && m_toolChangeTime.elapsed() > 250) {
			const QKeyEvent *e = static_cast<const QKeyEvent*>(event);
			if(!e->isAutoRepeat()) {
				if(m_tempToolSwitchShortcut->isShortcutSent()) {
					if(e->modifiers() == Qt::NoModifier) {
						// Return from temporary tool change
						for(const QAction *act : m_drawingtools->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && e->key() == seq[0]) {
								m_dockToolSettings->setPreviousTool();
								break;
							}
						}

						// Return from temporary tool slot change
						for(const QAction *act : m_brushSlots->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && e->key() == seq[0]) {
								m_dockToolSettings->setPreviousTool();
								break;
							}
						}
					}
				}

				m_tempToolSwitchShortcut->reset();
			}

		} else if(event->type() == QEvent::ShortcutOverride) {
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

		return QMainWindow::event(event);
	}
}

/**
 * Show the "new document" dialog
 */
void MainWindow::showNew()
{
	auto dlg = new dialogs::NewDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::NewDialog::accepted, this, &MainWindow::newDocument);
	dlg->show();
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
		if(recording::Reader::isRecordingExtension(file)) {
			DP_LoadResult result = m_doc->loadRecording(file);
			showLoadResultMessage(result);
			if(result == DP_LOAD_RESULT_SUCCESS) {
				QFileInfo fileinfo(file);
				m_playbackDialog = new dialogs::PlaybackDialog(m_doc->canvas(), this);
				m_playbackDialog->setWindowTitle(fileinfo.baseName() + " - " + m_playbackDialog->windowTitle());
				m_playbackDialog->setAttribute(Qt::WA_DeleteOnClose);
				m_playbackDialog->show();
				m_playbackDialog->centerOnParent();
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
	const QString file = QFileDialog::getOpenFileName(
		this,
		tr("Open Image"),
		getLastPath(),
		utils::fileFormatFilter(utils::FileFormatOption::OpenEverything)
	);

	const QUrl url = QUrl::fromLocalFile(file);
	if(url.isValid()) {
		setLastPath(file);
		open(url);
	}
}

/**
 * Allows the user three choices:
 * <ul>
 * <li>Cancel</li>
 * <li>Go ahead and flatten the image, then save<li>
 * <li>Save in OpenRaster format instead</li>
 * </ul>
 * If user chooces to save in OpenRaster, the suffix of file parameter is
 * altered.
 * @param file file name (may be altered)
 * @return true if file should be saved
 */
bool MainWindow::confirmFlatten(QString& file) const
{
	QMessageBox box(QMessageBox::Information, tr("Save Image"),
			tr("The selected format does not support layers or annotations."),
			QMessageBox::Cancel);
	box.addButton(tr("Flatten"), QMessageBox::AcceptRole);
	QPushButton *saveora = box.addButton(tr("Save as OpenRaster"), QMessageBox::ActionRole);

	// Don't save at all
	if(box.exec() == QMessageBox::Cancel)
		return false;

	// Save
	if(box.clickedButton() == saveora) {
		file = file.left(file.lastIndexOf('.')) + ".ora";
	}
	return true;
}

/**
 * If no file name has been selected, \a saveas is called.
 */
void MainWindow::save()
{
	QString filename = m_doc->currentFilename();

	if(filename.isEmpty() || !utils::isWritableFormat(filename)) {
		saveas();
		return;
	}

	if(!filename.endsWith("ora", Qt::CaseInsensitive) && m_doc->canvas()->paintEngine()->needsOpenRaster()) {
		// Note: the user may decide to save an ORA file instead, in which case the name is changed
		if(confirmFlatten(filename)==false)
			return;
	}

	// Overwrite current file
	m_doc->saveCanvas(filename);

	addRecentFile(filename);
}

/**
 * A standard file dialog is used to get the name of the file to save.
 * If no suffix is the suffix from the current filter is used.
 */
void MainWindow::saveas()
{
	QString selfilter;

	QString file = QFileDialog::getSaveFileName(
			this,
			tr("Save Image"),
			getLastPath(),
			utils::fileFormatFilter(utils::FileFormatOption::SaveImages),
			&selfilter);

	if(!file.isEmpty()) {

		// Set file suffix if missing
		const QFileInfo info(file);
		if(info.suffix().isEmpty()) {
			if(selfilter.isEmpty()) {
				// If we don't have selfilter, pick what is best
				if(m_doc->canvas()->paintEngine()->needsOpenRaster())
					file += ".ora";
				else
					file += ".png";
			} else {
				// Use the currently selected filter
				int i = selfilter.indexOf("*.")+1;
				int i2 = selfilter.indexOf(')', i);
				file += selfilter.mid(i, i2-i);
			}
		}

		// Confirm format choice if saving would result in flattening layers
		if(m_doc->canvas()->paintEngine()->needsOpenRaster() && !file.endsWith(".ora", Qt::CaseInsensitive)) {
			if(confirmFlatten(file)==false)
				return;
		}

		// Save the image
		setLastPath(file);
		m_doc->saveCanvas(file);
		addRecentFile(file);
	}
}

void MainWindow::saveSelection()
{
	QString selfilter = "PNG (*.png)";

	QString file = QFileDialog::getSaveFileName(
			this,
			tr("Save Image"),
			getLastPath(),
			utils::fileFormatFilter(utils::FileFormatOption::SaveImages | utils::FileFormatOption::QtImagesOnly),
			&selfilter);

	if(!file.isEmpty()) {
		// Set file suffix if missing
		const QFileInfo info(file);
		if(info.suffix().isEmpty()) {
			if(selfilter.isEmpty()) {
				file += ".png";
			} else {
				// Use the currently selected filter
				int i = selfilter.indexOf("*.")+1;
				int i2 = selfilter.indexOf(')', i);
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 2)
				file += selfilter.mid(i, i2-i);
#else
				file += QStringView{selfilter}.mid(i, i2-i);
#endif
			}
		}

		// Save the image
		setLastPath(file);
		m_doc->saveSelection(file);
	}
}

void MainWindow::onCanvasSaveStarted()
{
	QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
	getAction("savedocument")->setEnabled(false);
	getAction("savedocumentas")->setEnabled(false);
	m_viewStatusBar->showMessage(tr("Saving..."));

}

void MainWindow::onCanvasSaved(const QString &errorMessage)
{
	QApplication::restoreOverrideCursor();
	getAction("savedocument")->setEnabled(true);
	getAction("savedocumentas")->setEnabled(true);

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

static QString withSuffix(QString path, const QString &wantedSuffix)
{
	if(!path.isEmpty()) {
		QFileInfo info{path};
		QString suffix = info.suffix();
		if(suffix.compare(wantedSuffix, Qt::CaseInsensitive) != 0) {
			path.chop(suffix.length());
			if(!path.endsWith(".")) {
				path.append(".");
			}
			path.append(wantedSuffix);
		}
	}
	return path;
}

void MainWindow::exportGifAnimation()
{
	QString path = QFileDialog::getSaveFileName(
		this,
		tr("Export Animated GIF"),
		withSuffix(getLastPath(), "gif"),
		"GIF (*.gif)"
	);

	exportAnimation(withSuffix(path, "gif"), DP_save_animation_gif);
}

void MainWindow::exportAnimationFrames()
{
	QString lastPath = getLastPath();
	if(!lastPath.isEmpty()) {
		QFileInfo info{lastPath};
		if(!info.isDir()) {
			lastPath = info.dir().path();
		}
	}

	const QString path = QFileDialog::getExistingDirectory(
		this,
		tr("Choose folder to save frames in"),
		lastPath
	);

	exportAnimation(path, DP_save_animation_frames);
}

void MainWindow::exportAnimation(const QString &path, AnimationSaverRunnable::SaveFn saveFn)
{
	if(path.isEmpty())
		return;

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
	dialogs::Flipbook *fp = new dialogs::Flipbook(this);
	fp->setAttribute(Qt::WA_DeleteOnClose);
	fp->setPaintEngine(m_doc->canvas()->paintEngine());
	fp->show();
}

void MainWindow::setRecorderStatus(bool on)
{
	QAction *recordAction = getAction("recordsession");

	if(m_playbackDialog) {
		if(m_playbackDialog->isPlaying()) {
			recordAction->setIcon(icon::fromTheme("media-playback-pause"));
			recordAction->setText(tr("Pause"));
		} else {
			recordAction->setIcon(icon::fromTheme("media-playback-start"));
			recordAction->setText(tr("Play"));
		}

	} else {
		if(on) {
			recordAction->setText(tr("Stop Recording"));
			recordAction->setIcon(icon::fromTheme("media-playback-stop"));
		} else {
			recordAction->setText(tr("Record..."));
			recordAction->setIcon(icon::fromTheme("media-record"));
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

	QString file = QFileDialog::getSaveFileName(
		this,
		tr("Record Session"),
		getLastPath(),
		utils::fileFormatFilter(utils::FileFormatOption::SaveRecordings)
	);

	if(!file.isEmpty()) {
		drawdance::RecordStartResult result = m_doc->startRecording(file);
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
	QAction *profileAction = getAction("profile");
	if(drawdance::Perf::isOpen()) {
		if(!drawdance::Perf::close()) {
			showErrorMessageWithDetails(tr("Error closing profile."), DP_error());
		}
		profileAction->setText(tr("Profile..."));
	} else {
		QString path = QFileDialog::getSaveFileName(
			this,
			tr("Profile"),
			withSuffix(getLastPath(), "dpperf"),
			utils::fileFormatFilter(utils::FileFormatOption::SaveProfile)
		);
		if(!path.isEmpty()) {
			if(drawdance::Perf::open(withSuffix(path, "dpperf"))) {
				profileAction->setText(tr("Stop Profile"));
			} else {
				showErrorMessageWithDetails(tr("Error opening profile."), DP_error());
			}
		}
	}
}

/**
 * The settings window will be window modal and automatically destruct
 * when it is closed.
 */
void MainWindow::showSettings()
{
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog;
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::ApplicationModal);
	dlg->show();
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
	dlg->show();
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
		dlg->show();
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
	login->setAnnounceUrl(dlg->getAnnouncementUrl(), dlg->getAnnouncmentPrivate());
	if(useremote) {
		login->setInitialState(m_doc->canvas()->generateSnapshot());
	}

	(new dialogs::LoginDialog(login, this))->show();

	m_doc->client()->connectToServer(login);
}

/**
 * Show the join dialog
 */
void MainWindow::join(const QUrl &url)
{
	auto dlg = new dialogs::JoinDialog(url, this);

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
	dlg->show();
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

	dlg->show();
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
	dialogs::ResetDialog *dlg = new dialogs::ResetDialog(m_doc->canvas()->paintEngine(), this);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	// It's always possible to create a new document from a snapshot
	connect(dlg, &dialogs::ResetDialog::newSelected, this, [dlg]() {
		MainWindow *w = new MainWindow(false);
		w->m_doc->sendResetSession(dlg->getResetImage());
		dlg->deleteLater();
	});

	// Session resetting is available only to session operators
	if(m_doc->canvas()->aclState()->amOperator()) {
		connect(dlg, &dialogs::ResetDialog::resetSelected, this, [this, dlg]() {
			canvas::CanvasModel *canvas = m_doc->canvas();
			if(canvas->aclState()->amOperator()) {
				drawdance::MessageList snapshot = dlg->getResetImage();
				canvas->amendSnapshotMetadata(snapshot);
				m_doc->sendResetSession(snapshot);
			}
			dlg->deleteLater();
		});

	} else {
		dlg->setCanReset(false);
	}

	dlg->show();
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

	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::Mode::Join, url, this);
	auto *dlg = new dialogs::LoginDialog(login, this);
	connect(m_doc, &Document::catchupProgress, dlg, &dialogs::LoginDialog::catchupProgress);
	connect(m_doc, &Document::serverLoggedIn, dlg, [dlg,this](bool join) {
		dlg->onLoginDone(join);
		m_canvasscene->hideCanvas();
	});
	connect(dlg, &dialogs::LoginDialog::destroyed, m_canvasscene, &drawingboard::CanvasScene::showCanvas);

	dlg->show();
	m_doc->setRecordOnConnect(autoRecordFile);
	m_doc->client()->connectToServer(login);
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

void MainWindow::updateLockWidget()
{
	bool locked = m_doc->canvas() && m_doc->canvas()->aclState()->amLocked();
	getAction("locksession")->setChecked(locked);

	locked |= m_dockLayers->isCurrentLayerLocked();

	if(locked) {
		m_lockstatus->setPixmap(icon::fromTheme("object-locked").pixmap(16, 16));
		m_lockstatus->setToolTip(tr("Board is locked"));
	} else {
		m_lockstatus->setPixmap(QPixmap());
		m_lockstatus->setToolTip(QString());
	}
	m_view->setLocked(locked);
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
	default: break;
	}
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
	writeSettings();
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
 * and restored when leaving
 *
 * @param enable
 */
void MainWindow::toggleFullscreen()
{
	if(windowState().testFlag(Qt::WindowFullScreen)==false) {
		// Save windowed mode state
		m_fullscreenOldGeometry = geometry();
		m_fullscreenOldMaximized = isMaximized();

		menuBar()->hide();
		m_view->setFrameShape(QFrame::NoFrame);
		m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

		showFullScreen();

	} else {
		// Restore old state
		if(m_fullscreenOldMaximized) {
			showMaximized();
		} else {
			showNormal();
			setGeometry(m_fullscreenOldGeometry);
		}
		menuBar()->show();
		m_view->setFrameShape(QFrame::StyledPanel);

		m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
	}
}

void MainWindow::hotBorderMenubar(bool show)
{
	if(windowState().testFlag(Qt::WindowFullScreen)) {
		menuBar()->setVisible(show);
	}
}

void MainWindow::setFreezeDocks(bool freeze)
{
	const auto features = QDockWidget::DockWidgetClosable|QDockWidget::DockWidgetMovable|QDockWidget::DockWidgetFloatable;
	for(QObject *c : children()) {
		QDockWidget *dw = qobject_cast<QDockWidget*>(c);
		if(dw) {
			if(freeze)
				dw->setFeatures(dw->features() & ~features);
			else
				dw->setFeatures(dw->features() | features);
		}
	}
}

void MainWindow::setDocksHidden(bool hidden)
{
	int xOffset1=0, xOffset2=0, yOffset=0;

	for(QObject *c : children()) {
		QWidget *w = qobject_cast<QWidget*>(c);
		if(w && (w->inherits("QDockWidget") || w->inherits("QToolBar"))) {
			bool visible = w->isVisible();

			if(hidden) {
				w->setProperty("wasVisible", w->isVisible());
				w->hide();
			} else {
				const QVariant v = w->property("wasVisible");
				if(!v.isNull()) {
					w->setVisible(v.toBool());
					visible = v.toBool();
				}
			}

			QToolBar *tb = qobject_cast<QToolBar*>(w);
			if(tb && visible && !tb->isFloating()) {
				if(toolBarArea(tb) == Qt::TopToolBarArea)
					yOffset = tb->height();
				else if(toolBarArea(tb) == Qt::LeftToolBarArea)
					xOffset1 = tb->width();
			}

			QDockWidget *dw = qobject_cast<QDockWidget*>(w);
			if(dw && visible && !dw->isFloating() && dockWidgetArea(dw) == Qt::LeftDockWidgetArea)
				xOffset2 = dw->width();
		}
	}

	m_viewStatusBar->setHidden(hidden);

	// Docks can only dock on the left or right, so only one yOffset is needed.
	const int dir = hidden ? -1 : 1;
	m_view->scrollBy(dir * (xOffset1+xOffset2), dir * yOffset);
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
		if(QSettings().value("settings/tooltoggle", true).toBool())
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
	m_view->setPointerTracking(tool==tools::Tool::LASERPOINTER && static_cast<tools::LaserPointerSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::LASERPOINTER))->pointerTracking());

	// Remove selection when not using selection tool
	if(tool != tools::Tool::SELECTION && tool != tools::Tool::POLYGONSELECTION)
		m_doc->selectNone();

	// Deselect annotation when tool changed
	if(tool != tools::Tool::ANNOTATION)
		m_doc->toolCtrl()->setActiveAnnotation(0);

	m_doc->toolCtrl()->setActiveTool(tool);
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
	const QMimeData *data = QApplication::clipboard()->mimeData();
	if(data->hasImage()) {
		QPoint pastepos;
		bool pasteAtPos = false;

		// Get source position
		QByteArray srcpos = data->data("x-drawpile/pastesrc");
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
			pasteImage(data->imageData().value<QImage>(), &pastepos, true);
		else
			pasteImage(data->imageData().value<QImage>());
	}
}

void MainWindow::pasteCentered()
{
	const QMimeData *data = QApplication::clipboard()->mimeData();
	if(data->hasImage()) {
		pasteImage(data->imageData().value<QImage>(), nullptr, true);
	}
}

void MainWindow::pasteFile()
{
	const QString file = QFileDialog::getOpenFileName(
		this,
		tr("Paste Image"),
		getLastPath(),
		// note: Only Qt's native image formats are supported at the moment
		utils::fileFormatFilter(utils::FileFormatOption::OpenImages | utils::FileFormatOption::QtImagesOnly)
	);

	if(file.isEmpty()==false) {
		const QFileInfo info(file);
		setLastPath(info.absolutePath());

		pasteFile(QUrl::fromLocalFile(file));
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
		const uint16_t a = static_cast<tools::AnnotationSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::ANNOTATION))->selected();
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
	m_doc->fillArea(Qt::white, DP_BLEND_MODE_ERASE);
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
	dlg->show();
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
	auto *dlg = new color_widgets::ColorDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setColor(m_doc->canvas()->paintEngine()->backgroundColor());

	connect(dlg, &color_widgets::ColorDialog::colorSelected, m_doc, &Document::sendCanvasBackground);
	dlg->show();
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

	color_widgets::ColorDialog *dlg = new color_widgets::ColorDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setColor(color);

	connect(
		dlg, &color_widgets::ColorDialog::colorSelected, paintEngine,
		&canvas::PaintEngine::setLocalBackgroundColor);
	dlg->show();
}

void MainWindow::clearLocalCanvasBackground()
{
	if(!m_doc->canvas()) {
		qWarning("clearLocalCanvasBackground: no canvas!");
		return;
	}
	m_doc->canvas()->paintEngine()->clearLocalBackgroundColor();
}

void MainWindow::about()
{
#ifdef BUILD_LABEL
	const QString version = DRAWPILE_VERSION " (" BUILD_LABEL ")";
#else
	const QString version = DRAWPILE_VERSION;
#endif
	QMessageBox::about(nullptr, tr("About Drawpile"),
			QStringLiteral("<p><b>Drawpile %1</b><br>").arg(version) +
			tr("A collaborative drawing program.") + QStringLiteral("</p>"

			"<p>Copyright © 2006-2022 Calle Laakkonen</p>"

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
			QStringLiteral("<hr><dl><dt><b>%1</b></dt><dd>%2</dd></dl>")
				.arg(tr("Settings File:"))
				.arg(QSettings{}.fileName().toHtmlEscaped())
	);
}

void MainWindow::homepage()
{
	QDesktopServices::openUrl(QUrl(WEBSITE));
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

	// Collect list of docks for dock menu
	for(QObject *c : children()) {
		QDockWidget *dw = qobject_cast<QDockWidget*>(c);
		if(dw)
			toggledockmenu->addAction(dw->toggleViewAction());
	}

	toggledockmenu->addSeparator();
	QAction *freezeDocks = makeAction("freezedocks", tr("Lock in place")).checkable().remembered();
	toggledockmenu->addAction(freezeDocks);
	connect(freezeDocks, &QAction::toggled, this, &MainWindow::setFreezeDocks);

	QAction *hideDocks = makeAction("hidedocks", tr("Hide Docks")).checkable().shortcut("tab");
	toggledockmenu->addAction(hideDocks);
	connect(hideDocks, &QAction::toggled, this, &MainWindow::setDocksHidden);

	//
	// File menu and toolbar
	//
	QAction *newdocument = makeAction("newdocument", tr("&New")).icon("document-new").shortcut(QKeySequence::New);
	QAction *open = makeAction("opendocument", tr("&Open...")).icon("document-open").shortcut(QKeySequence::Open);
#ifdef Q_OS_MAC
	QAction *closefile = makeAction("closedocument", tr("Close")).shortcut(QKeySequence::Close);
#endif
	QAction *save = makeAction("savedocument", tr("&Save")).icon("document-save").shortcut(QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", tr("Save &As...")).icon("document-save-as").shortcut(QKeySequence::SaveAs);
	QAction *savesel = makeAction("saveselection", tr("Save Selection...")).icon("document-save-as");
	QAction *autosave = makeAction("autosave", tr("Autosave")).checkable().disabled();
	QAction *exportGifAnimation = makeAction("exportanimgif", tr("Animated &GIF..."));
	QAction *exportAnimationFrames = makeAction("exportanimframes", tr("Animation &Frames..."));

	QAction *record = makeAction("recordsession", tr("Record...")).icon("media-record");
	QAction *profile = makeAction("profile", tr("Profile..."));
	QAction *quit = makeAction("exitprogram", tr("&Quit")).icon("application-exit").shortcut("Ctrl+Q").menuRole(QAction::QuitRole);

#ifdef Q_OS_MAC
	m_currentdoctools->addAction(closefile);
#endif
	m_currentdoctools->addAction(save);
	m_currentdoctools->addAction(saveas);
	m_currentdoctools->addAction(savesel);
	m_currentdoctools->addAction(exportGifAnimation);
	m_currentdoctools->addAction(exportAnimationFrames);
	m_currentdoctools->addAction(record);

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));
	connect(savesel, &QAction::triggered, this, &MainWindow::saveSelection);

	connect(autosave, &QAction::triggered, m_doc, &Document::setAutosave);
	connect(m_doc, &Document::autosaveChanged, autosave, &QAction::setChecked);
	connect(m_doc, &Document::canAutosaveChanged, autosave, &QAction::setEnabled);

	connect(exportGifAnimation, &QAction::triggered, this, &MainWindow::exportGifAnimation);
	connect(exportAnimationFrames, &QAction::triggered, this, &MainWindow::exportAnimationFrames);
	connect(record, &QAction::triggered, this, &MainWindow::toggleRecording);
	connect(profile, &QAction::triggered, this, &MainWindow::toggleProfile);
#ifdef Q_OS_MAC
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

#ifdef Q_OS_MAC
	filemenu->addAction(closefile);
#endif
	filemenu->addAction(save);
	filemenu->addAction(saveas);
	filemenu->addAction(savesel);
	filemenu->addAction(autosave);
	filemenu->addSeparator();

	QMenu *exportMenu = filemenu->addMenu(tr("&Export"));
	exportMenu->setIcon(icon::fromTheme("document-export"));
	exportMenu->addAction(exportGifAnimation);
	exportMenu->addAction(exportAnimationFrames);
	filemenu->addAction(record);
	filemenu->addAction(profile);
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

	QAction *pastefile = makeAction("pastefile", tr("Paste &From File...")).icon("document-open");
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", tr("Delete Empty Annotations"));
	QAction *resize = makeAction("resizecanvas", tr("Resi&ze Canvas..."));
	QAction *canvasBackground = makeAction("canvas-background", tr("Set Session Background..."));
	QAction *setLocalBackground = makeAction("set-local-background", tr("Set Local Background..."));
	QAction *clearLocalBackground = makeAction("clear-local-background", tr("Clear Local Background"));
	QAction *preferences = makeAction("preferences", tr("Prefere&nces")).menuRole(QAction::PreferencesRole);

	QAction *selectall = makeAction("selectall", tr("Select &All")).shortcut(QKeySequence::SelectAll);
	QAction *selectnone = makeAction("selectnone", tr("&Deselect"))
#if (defined(Q_OS_MAC) || defined(Q_OS_WIN)) // Deselect is not defined on Mac and Win
		.shortcut("Shift+Ctrl+A")
#else
		.shortcut(QKeySequence::Deselect)
#endif
	;

	QAction *expandup = makeAction("expandup", tr("Expand &Up")).shortcut(CTRL_KEY "+J");
	QAction *expanddown = makeAction("expanddown", tr("Expand &Down")).shortcut(CTRL_KEY "+K");
	QAction *expandleft = makeAction("expandleft", tr("Expand &Left")).shortcut(CTRL_KEY "+H");
	QAction *expandright = makeAction("expandright", tr("Expand &Right")).shortcut(CTRL_KEY "+L");

	QAction *cleararea = makeAction("cleararea", tr("Delete")).shortcut("Delete");
	QAction *fillfgarea = makeAction("fillfgarea", tr("Fill Selection")).shortcut(CTRL_KEY "+,");
	QAction *recolorarea = makeAction("recolorarea", tr("Recolor Selection")).shortcut(CTRL_KEY "+Shift+,");
	QAction *colorerasearea = makeAction("colorerasearea", tr("Color Erase Selection")).shortcut("Shift+Delete");

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
	QAction *toolbartoggles = new QAction(tr("&Toolbars"), this);
	toolbartoggles->setMenu(toggletoolbarmenu);

	QAction *docktoggles = new QAction(tr("&Docks"), this);
	docktoggles->setMenu(toggledockmenu);

	QAction *toggleChat = makeAction("togglechat", tr("Chat")).shortcut("Alt+C").checked();

	QAction *showFlipbook = makeAction("showflipbook", tr("Flipbook")).statusTip(tr("Show animation preview window")).shortcut("Ctrl+F");

	QAction *zoomin = makeAction("zoomin", tr("Zoom &In")).icon("zoom-in").shortcut(QKeySequence::ZoomIn);
	QAction *zoomout = makeAction("zoomout", tr("Zoom &Out")).icon("zoom-out").shortcut(QKeySequence::ZoomOut);
	QAction *zoomorig = makeAction("zoomone", tr("&Normal Size")).icon("zoom-original").shortcut(QKeySequence(Qt::CTRL + Qt::Key_0));
	QAction *rotateorig = makeAction("rotatezero", tr("&Reset Rotation")).icon("transform-rotate").shortcut(QKeySequence(Qt::CTRL + Qt::Key_R));
	QAction *rotatecw = makeAction("rotatecw", tr("Rotate Canvas Clockwise")).shortcut(QKeySequence(Qt::SHIFT + Qt::Key_Period)).icon("object-rotate-right");
	QAction *rotateccw = makeAction("rotateccw", tr("Rotate Canvas Counterclockwise")).shortcut(QKeySequence(Qt::SHIFT + Qt::Key_Comma)).icon("object-rotate-left");

	QAction *viewmirror = makeAction("viewmirror", tr("Mirror")).icon("object-flip-horizontal").shortcut("V").checkable();
	QAction *viewflip = makeAction("viewflip", tr("Flip")).icon("object-flip-vertical").shortcut("C").checkable();

	QAction *showannotations = makeAction("showannotations", tr("Show &Annotations")).checked().remembered();
	QAction *showusermarkers = makeAction("showusermarkers", tr("Show User &Pointers")).checked().remembered();
	QAction *showusernames = makeAction("showmarkernames", tr("Show Names")).checked().remembered();
	QAction *showuserlayers = makeAction("showmarkerlayers", tr("Show Layers")).checked().remembered();
	QAction *showuseravatars = makeAction("showmarkeravatars", tr("Show Avatars")).checked().remembered();
	QAction *showlasers = makeAction("showlasers", tr("Show La&ser Trails")).checked().remembered();
	QAction *showgrid = makeAction("showgrid", tr("Show Pixel &Grid")).checked().remembered();

	QAction *fullscreen = makeAction("fullscreen", tr("&Full Screen")).shortcut(QKeySequence::FullScreen).checkable();

	m_currentdoctools->addAction(showFlipbook);

	if(windowHandle()) { // mainwindow should always be a native window, but better safe than sorry
		connect(windowHandle(), &QWindow::windowStateChanged, fullscreen, [fullscreen](Qt::WindowState state) {
			// Update the mode tickmark on fulscreen state change.
			// On Qt 5.3.0, this signal doesn't seem to get emitted on OSX when clicking
			// on the toggle button in the titlebar. The state can be queried correctly though.
			fullscreen->setChecked(state & Qt::WindowFullScreen);
		});
	}

	connect(m_statusChatButton, &QToolButton::clicked, toggleChat, &QAction::trigger);

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

	connect(showFlipbook, &QAction::triggered, this, &MainWindow::showFlipbook);

	connect(zoomin, &QAction::triggered, m_view, &widgets::CanvasView::zoomin);
	connect(zoomout, &QAction::triggered, m_view, &widgets::CanvasView::zoomout);
	connect(zoomorig, &QAction::triggered, this, [this]() { m_view->setZoom(100.0); });
	connect(rotateorig, &QAction::triggered, this, [this]() { m_view->setRotation(0); });
	connect(rotatecw, &QAction::triggered, this, [this]() { m_view->setRotation(m_view->rotation() + 5); });
	connect(rotateccw, &QAction::triggered, this, [this]() { m_view->setRotation(m_view->rotation() - 5); });
	connect(viewflip, SIGNAL(triggered(bool)), m_view, SLOT(setViewFlip(bool)));
	connect(viewmirror, SIGNAL(triggered(bool)), m_view, SLOT(setViewMirror(bool)));

	connect(fullscreen, &QAction::triggered, this, &MainWindow::toggleFullscreen);

	connect(showannotations, &QAction::toggled, this, &MainWindow::setShowAnnotations);
	connect(showusermarkers, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserMarkers);
	connect(showusernames, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserNames);
	connect(showuserlayers, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserLayers);
	connect(showuseravatars, &QAction::toggled, m_canvasscene, &drawingboard::CanvasScene::showUserAvatars);
	connect(showlasers, &QAction::toggled, this, &MainWindow::setShowLaserTrails);
	connect(showgrid, &QAction::toggled, m_view, &widgets::CanvasView::setPixelGrid);

	m_viewstatus->setActions(viewflip, viewmirror, rotateorig, zoomorig);

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(toolbartoggles);
	viewmenu->addAction(docktoggles);
	viewmenu->addAction(toggleChat);
	viewmenu->addAction(showFlipbook);
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

	QAction *layerViewNormal = makeAction("layerviewnormal", tr("Normal")).checkable().checked();
	QAction *layerViewCurrentLayer = makeAction("layerviewcurrentlayer", tr("Current Layer Only")).shortcut("Home").checkable();
	QAction *layerViewCurrentFrame = makeAction("layerviewcurrentframe", tr("Current Frame Only")).shortcut("Shift+Home").checkable();
	QAction *layerViewOnionSkin = makeAction("layerviewonionskin", tr("Onion Skin")).shortcut("Ctrl+Shift+O").checkable();
	QAction *layerUncensor = makeAction("layerviewuncensor", tr("Show Censored Layers")).checkable().remembered();
	m_lastLayerViewMode = layerViewNormal;

	QActionGroup *layerViewModeGroup = new QActionGroup(this);
	layerViewModeGroup->setExclusive(true);
	layerViewModeGroup->addAction(layerViewNormal);
	layerViewModeGroup->addAction(layerViewCurrentLayer);
	layerViewModeGroup->addAction(layerViewCurrentFrame);
	layerViewModeGroup->addAction(layerViewOnionSkin);

	QMenu *layerViewMenu = viewmenu->addMenu(tr("Layer View Mode"));
	layerViewMenu->addAction(layerViewNormal);
	layerViewMenu->addAction(layerViewCurrentLayer);
	layerViewMenu->addAction(layerViewCurrentFrame);
	layerViewMenu->addAction(layerViewOnionSkin);
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

	viewmenu->addSeparator();
	viewmenu->addAction(fullscreen);

	//
	// Layer menu
	//
	QAction *layerAdd = makeAction("layeradd", tr("New Layer")).shortcut("Shift+Ctrl+Insert").icon("list-add");
	QAction *groupAdd = makeAction("groupadd", tr("New Group")).icon("folder-new");
	QAction *layerDupe = makeAction("layerdupe", tr("Duplicate Layer")).icon("edit-copy");
	QAction *layerMerge = makeAction("layermerge", tr("Merge with Layer Below")).icon("arrow-down-double");
	QAction *layerProperties = makeAction("layerproperties", tr("Properties...")).icon("configure");
	QAction *layerDelete = makeAction("layerdelete", tr("Delete Layer")).icon("list-remove");

	m_dockLayers->setLayerEditActions(layerAdd, groupAdd, layerDupe, layerMerge, layerProperties, layerDelete);

	QAction *layerUpAct = makeAction("layer-up", tr("Select Above")).shortcut("Shift+X");
	QAction *layerDownAct = makeAction("layer-down", tr("Select Below")).shortcut("Shift+Z");
	QAction *nextFrameAct = makeAction("frame-next", tr("Next Frame")).shortcut("Ctrl+Shift+X");
	QAction *prevFrameAct = makeAction("frame-prev", tr("Previous Frame")).shortcut("Ctrl+Shift+Z");

	connect(layerUpAct, &QAction::triggered, m_dockLayers, &docks::LayerList::selectAbove);
	connect(layerDownAct, &QAction::triggered, m_dockLayers, &docks::LayerList::selectBelow);
	connect(nextFrameAct, &QAction::triggered, m_dockTimeline, &docks::Timeline::setNextFrame);
	connect(prevFrameAct, &QAction::triggered, m_dockTimeline, &docks::Timeline::setPreviousFrame);

	QMenu *layerMenu = menuBar()->addMenu(tr("Layer"));
	layerMenu->addAction(layerAdd);
	layerMenu->addAction(groupAdd);
	layerMenu->addAction(layerDupe);
	layerMenu->addAction(layerMerge);
	layerMenu->addAction(layerDelete);

	layerMenu->addSeparator();
	layerMenu->addAction(layerUncensor);

	layerMenu->addSeparator();
	layerMenu->addAction(layerUpAct);
	layerMenu->addAction(layerDownAct);

	layerMenu->addSeparator();
	layerMenu->addAction(nextFrameAct);
	layerMenu->addAction(prevFrameAct);



	//
	// Session menu
	//
	QAction *host = makeAction("hostsession", tr("&Host...")).statusTip(tr("Share your drawingboard with others"));
	QAction *join = makeAction("joinsession", tr("&Join...")).statusTip(tr("Join another user's drawing session"));
	QAction *logout = makeAction("leavesession", tr("&Leave")).statusTip(tr("Leave this drawing session")).disabled();

	QAction *serverlog = makeAction("viewserverlog", tr("Event Log")).noDefaultShortcut();
	QAction *sessionSettings = makeAction("sessionsettings", tr("Settings...")).noDefaultShortcut().menuRole(QAction::NoRole).disabled();

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
	connect(sessionSettings, &QAction::triggered, m_sessionSettings, &dialogs::SessionSettingsDialog::show);
	connect(serverlog, &QAction::triggered, m_serverLogDialog, &dialogs::ServerLogDialog::show);
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

	QAction *currentEraseMode = makeAction("currenterasemode", tr("Toggle eraser mode")).shortcut("Ctrl+E");
	QAction *swapcolors = makeAction("swapcolors", tr("Swap Last Colors")).shortcut("X");
	QAction *smallerbrush = makeAction("ensmallenbrush", tr("&Decrease Brush Size")).shortcut(Qt::Key_BracketLeft);
	QAction *biggerbrush = makeAction("embiggenbrush", tr("&Increase Brush Size")).shortcut(Qt::Key_BracketRight);

	smallerbrush->setAutoRepeat(true);
	biggerbrush->setAutoRepeat(true);

	connect(currentEraseMode, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::toggleEraserMode);
	connect(swapcolors, &QAction::triggered, m_dockToolSettings, &docks::ToolSettings::swapLastUsedColors);

	connect(smallerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(false); });
	connect(biggerbrush, &QAction::triggered, this, [this]() { m_dockToolSettings->stepAdjustCurrent1(true); });

	toolshortcuts->addAction(currentEraseMode);
	toolshortcuts->addAction(swapcolors);
	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);

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
#ifdef Q_OS_MAC
	menuBar()->addMenu(MacMenu::instance()->windowMenu());
#endif

	//
	// Help menu
	//
	QAction *homepage = makeAction("dphomepage", tr("&Homepage")).statusTip(WEBSITE);
	QAction *tablettester = makeAction("tablettester", tr("Tablet Tester"));
	QAction *showlogfile = makeAction("showlogfile", tr("Log File"));
	QAction *about = makeAction("dpabout", tr("&About Drawpile")).menuRole(QAction::AboutRole);
	QAction *aboutqt = makeAction("aboutqt", tr("About &Qt")).menuRole(QAction::AboutQtRole);
	QAction *versioncheck = makeAction("versioncheck", tr("Check For Updates"));

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);
	connect(versioncheck, &QAction::triggered, this, [this]() {
		auto *dlg = new dialogs::VersionCheckDialog(this);
		dlg->show();
		dlg->queryNewVersions();
	});

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
		ttd->show();
		ttd->raise();
	});

	connect(showlogfile, &QAction::triggered, []() {
		QDesktopServices::openUrl(QUrl::fromLocalFile(utils::logFilePath()));
	});

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addAction(tablettester);
	helpmenu->addAction(showlogfile);
	helpmenu->addSeparator();
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);
	helpmenu->addSeparator();
	helpmenu->addAction(versioncheck);

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
}

void MainWindow::createDocks()
{
	Q_ASSERT(m_doc);
	Q_ASSERT(m_view);
	Q_ASSERT(m_canvasscene);

	// Create tool settings
	m_dockToolSettings = new docks::ToolSettings(m_doc->toolCtrl(), this);
	m_dockToolSettings->setObjectName("ToolSettings");
	m_dockToolSettings->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_dockToolSettings);
	static_cast<tools::SelectionSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::SELECTION))->setView(m_view);
	static_cast<tools::AnnotationSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::ANNOTATION))->setScene(m_canvasscene);
	m_view->setPressureMapping(static_cast<tools::BrushSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::FREEHAND))->getPressureMapping());

	// Create brush palette
	m_dockBrushPalette = new docks::BrushPalette(this);
	m_dockBrushPalette->setObjectName("BrushPalette");
	m_dockBrushPalette->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_dockBrushPalette);

	tools::BrushSettings *brushSettings = static_cast<tools::BrushSettings*>(m_dockToolSettings->getToolSettingsPage(tools::Tool::FREEHAND));
	m_dockBrushPalette->connectBrushSettings(brushSettings);

	// Create color docks
	m_dockColorSpinner = new docks::ColorSpinnerDock(tr("Color Wheel"), this);
	m_dockColorSpinner->setObjectName("colorspinnerdock");
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSpinner);

	m_dockColorPalette = new docks::ColorPaletteDock(tr("Palette"), this);
	m_dockColorPalette->setObjectName("colorpalettedock");
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorPalette);

	m_dockColorSliders = new docks::ColorSliderDock(tr("Color Sliders"), this);
	m_dockColorSliders->setObjectName("colorsliderdock");
	addDockWidget(Qt::RightDockWidgetArea, m_dockColorSliders);

	tabifyDockWidget(m_dockColorPalette, m_dockColorSliders);
	tabifyDockWidget(m_dockColorSliders, m_dockColorSpinner);

	// Create layer list
	m_dockLayers = new docks::LayerList(this);
	m_dockLayers->setObjectName("LayerList");
	m_dockLayers->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_dockLayers);

	// Create navigator
	m_dockNavigator = new docks::Navigator(this);
	m_dockNavigator->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_dockNavigator);
	m_dockNavigator->hide(); // hidden by default

	// Create onion skin settings
	m_dockOnionSkins = new docks::OnionSkinsDock(tr("Onion Skins"), this);
	m_dockOnionSkins->setObjectName("onionskins");
	m_dockOnionSkins->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::TopDockWidgetArea, m_dockOnionSkins);

	// Create timeline
	m_dockTimeline = new docks::Timeline(this);
	m_dockTimeline->setObjectName("Timeline");
	m_dockTimeline->setAllowedAreas(Qt::AllDockWidgetAreas);
	addDockWidget(Qt::TopDockWidgetArea, m_dockTimeline);

	tabifyDockWidget(m_dockOnionSkins, m_dockTimeline);
}
