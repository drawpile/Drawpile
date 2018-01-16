/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2018 Calle Laakkonen

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
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDesktopWidget>
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

#ifndef NDEBUG
#include "core/tile.h"
#endif

#ifdef Q_OS_OSX
#define CTRL_KEY "Meta"
#include "widgets/macmenu.h"
#else
#define CTRL_KEY "Ctrl"
#endif

#include "config.h"
#include "mainwindow.h"
#include "document.h"

#include "core/layerstack.h"
#include "canvas/loader.h"
#include "canvas/canvasmodel.h"
#include "scene/canvasview.h"
#include "scene/canvasscene.h"
#include "scene/selectionitem.h"
#include "canvas/statetracker.h"
#include "canvas/userlist.h"

#include "utils/recentfiles.h"
#include "../shared/util/whatismyip.h"
#include "utils/icon.h"
#include "utils/images.h"
#include "utils/netfiles.h"
#include "utils/shortcutdetector.h"
#include "utils/customshortcutmodel.h"
#include "utils/settings.h"
#include "utils/logging.h"

#include "widgets/viewstatus.h"
#include "widgets/netstatus.h"
#include "widgets/chatwidget.h"
#include "widgets/userlistwidget.h"

#include "docks/toolsettingsdock.h"
#include "docks/brushpalettedock.h"
#include "docks/navigator.h"
#include "docks/colorbox.h"
#include "docks/layerlistdock.h"
#include "docks/inputsettingsdock.h"

#include "net/client.h"
#include "net/commands.h"
#include "net/login.h"
#include "net/serverthread.h"
#include "canvas/layerlist.h"
#include "canvas/aclfilter.h"
#include "parentalcontrols/parentalcontrols.h"

#include "tools/toolcontroller.h"
#include "toolwidgets/colorpickersettings.h"
#include "toolwidgets/selectionsettings.h"
#include "toolwidgets/annotationsettings.h"
#include "toolwidgets/lasersettings.h"

#include "../shared/record/reader.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/logindialog.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/resizedialog.h"
#include "dialogs/playbackdialog.h"
#include "dialogs/flipbook.h"
#include "dialogs/videoexportdialog.h"
#include "dialogs/resetdialog.h"
#include "dialogs/sessionsettings.h"
#include "dialogs/serverlogdialog.h"
#include "dialogs/tablettester.h"
#include "dialogs/abusereport.h"

#include "export/animation.h"
#include "export/videoexporter.h"

namespace {

QString getLastPath() {
	QFileInfo fi(QSettings().value("window/lastpath").toString());
	return fi.absoluteDir().absolutePath();
}

void setLastPath(const QString &lastpath) {
	QSettings cfg;
	cfg.setValue("window/lastpath", lastpath);
}

}

MainWindow::MainWindow(bool restoreWindowPosition)
	: QMainWindow(), m_playbackDialog(nullptr),
	  _canvasscene(nullptr), _lastToolBeforePaste(-1),
	  m_exitAfterSave(false)
{
	// The document (initially empty)
	m_doc = new Document(this);

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

	m_sessionSettings = new dialogs::SessionSettingsDialog(m_doc, this);

	m_serverLogDialog = new dialogs::ServerLogDialog(this);
	m_serverLogDialog->setModel(m_doc->serverLog());

	// The central widget consists of a custom status bar and a splitter
	// which includes the chat box and the main view.
	// We don't use the normal QMainWindow statusbar to save some vertical space for the docks.
	QWidget *centralwidget = new QWidget;
	QVBoxLayout *mainwinlayout = new QVBoxLayout(centralwidget);
	mainwinlayout->setContentsMargins(0, 0, 0 ,0);
	mainwinlayout->setSpacing(0);
	setCentralWidget(centralwidget);

	updateTitle();

	createDocks();

	setUnifiedTitleAndToolBarOnMac(true);

	_tempToolSwitchShortcut = new ShortcutDetector(this);

	// Work area is split between the canvas view and the chatbox
	_splitter = new QSplitter(Qt::Vertical, centralwidget);

	mainwinlayout->addWidget(_splitter);

	// Create custom status bar
	m_viewStatusBar = new QStatusBar;
	m_viewStatusBar->setSizeGripEnabled(false);
	mainwinlayout->addWidget(m_viewStatusBar);

	// Create status indicator widgets
	_viewstatus = new widgets::ViewStatus(this);

	m_netstatus = new widgets::NetStatus(this);
	_recorderstatus = new QLabel(this);
	_recorderstatus->hide();
	_lockstatus = new QLabel(this);
	_lockstatus->setFixedSize(QSize(16, 16));

	// Statusbar chat button: this is normally hidden and only shown
	// when there are unread chat messages.
	_statusChatButton = new QToolButton(this);
	_statusChatButton->setAutoRaise(true);
	_statusChatButton->setIcon(QIcon("builtin:chat.svg"));
	_statusChatButton->hide();
	m_viewStatusBar->addWidget(_statusChatButton);

	// Statusbar session size label
	QLabel *sessionHistorySize = new QLabel(this);
	m_viewStatusBar->addWidget(sessionHistorySize);

#ifndef NDEBUG
	// Debugging tool: show amount of memory consumed by tiles
	{
		QLabel *tilemem = new QLabel(this);
		QTimer *tilememtimer = new QTimer(this);
		connect(tilememtimer, &QTimer::timeout, [tilemem]() {
			tilemem->setText(QStringLiteral("Tiles: %1 Mb").arg(paintcore::TileData::megabytesUsed(), 0, 'f', 2));
		});
		tilememtimer->setInterval(1000);
		tilememtimer->start(1000);
		m_viewStatusBar->addPermanentWidget(tilemem);
	}
#endif

	m_viewStatusBar->addPermanentWidget(_viewstatus);
	m_viewStatusBar->addPermanentWidget(m_netstatus);
	m_viewStatusBar->addPermanentWidget(_recorderstatus);
	m_viewStatusBar->addPermanentWidget(_lockstatus);

	int SPLITTER_WIDGET_IDX = 0;

	// Create canvas view
	_view = new widgets::CanvasView(this);
	
	connect(static_cast<tools::LaserPointerSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::LASERPOINTER)), &tools::LaserPointerSettings::pointerTrackingToggled,
		_view, &widgets::CanvasView::setPointerTracking);

	connect(_dock_input, &docks::InputSettings::pressureMappingChanged, _view, &widgets::CanvasView::setPressureMapping);
	_view->setPressureMapping(_dock_input->getPressureMapping());

	connect(_dock_layers, &docks::LayerList::layerSelected, this, &MainWindow::updateLockWidget);
	connect(_dock_layers, &docks::LayerList::activeLayerVisibilityChanged, this, &MainWindow::updateLockWidget);

	_splitter->addWidget(_view);
	_splitter->setCollapsible(SPLITTER_WIDGET_IDX++, false);


	connect(_dock_toolsettings, SIGNAL(sizeChanged(int)), _view, SLOT(setOutlineSize(int)));
	connect(_dock_toolsettings, SIGNAL(subpixelModeChanged(bool)), _view, SLOT(setOutlineSubpixelMode(bool)));
	connect(_view, SIGNAL(colorDropped(QColor)), _dock_toolsettings, SLOT(setForegroundColor(QColor)));
	connect(_view, SIGNAL(imageDropped(QImage)), this, SLOT(pasteImage(QImage)));
	connect(_view, SIGNAL(urlDropped(QUrl)), this, SLOT(dropUrl(QUrl)));
	connect(_view, SIGNAL(viewTransformed(qreal, qreal)), _viewstatus, SLOT(setTransformation(qreal, qreal)));

#ifndef Q_OS_MAC // OSX provides this feature itself
	connect(_view, &widgets::CanvasView::hotBorder, this, &MainWindow::hotBorderMenubar);
#endif

	connect(_viewstatus, SIGNAL(zoomChanged(qreal)), _view, SLOT(setZoom(qreal)));
	connect(_viewstatus, SIGNAL(angleChanged(qreal)), _view, SLOT(setRotation(qreal)));

	connect(_dock_toolsettings, &docks::ToolSettings::toolChanged, this, &MainWindow::toolChanged);

	// Create the chatbox and user list
	QSplitter *chatsplitter = new QSplitter(Qt::Horizontal, this);
	chatsplitter->setChildrenCollapsible(false);
	m_chatbox = new widgets::ChatBox(this);
	chatsplitter->addWidget(m_chatbox);

	_userlist = new widgets::UserList(this);
	chatsplitter->addWidget(_userlist);

	chatsplitter->setStretchFactor(0, 5);
	chatsplitter->setStretchFactor(1, 1);
	_splitter->addWidget(chatsplitter);

	{
		// Nice initial division between canvas and chat
		const int h = height();
		_splitter->setSizes(QList<int>() << (h * 2 / 3) << (h / 3));
	}

	// Create canvas scene
	_canvasscene = new drawingboard::CanvasScene(this);
	_canvasscene->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	_view->setCanvas(_canvasscene);
	_dock_navigator->setScene(_canvasscene);

	// Color docks
	connect(_dock_toolsettings, SIGNAL(foregroundColorChanged(QColor)), _dock_colors, SLOT(setColor(QColor)));
	connect(_dock_colors, SIGNAL(colorChanged(QColor)), _dock_toolsettings, SLOT(setForegroundColor(QColor)));

	// Navigator <-> View
	connect(_dock_navigator, &docks::Navigator::focusMoved, _view, &widgets::CanvasView::scrollTo);
	connect(_view, &widgets::CanvasView::viewRectChange, _dock_navigator, &docks::Navigator::setViewFocus);
	connect(_dock_navigator, &docks::Navigator::wheelZoom, _view, &widgets::CanvasView::zoomSteps);


	// Network client <-> UI connections
	connect(_view, &widgets::CanvasView::pointerMoved, m_doc, &Document::sendPointerMove);

	connect(m_doc->client(), &net::Client::serverMessage, m_chatbox, &widgets::ChatBox::systemMessage);
	connect(m_doc->client(), &net::Client::serverMessage, m_netstatus, &widgets::NetStatus::alertMessage);
	connect(m_doc, &Document::catchupProgress, m_netstatus, &widgets::NetStatus::setCatchupProgress);

	connect(m_doc->client(), &net::Client::serverStatusUpdate, sessionHistorySize, [sessionHistorySize](int size) {
		sessionHistorySize->setText(QString("%1 MB").arg(size / float(1024*1024), 0, 'f', 2));
	});

	connect(m_chatbox, &widgets::ChatBox::message, m_doc->client(), &net::Client::sendMessage);

	static_cast<tools::SelectionSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::SELECTION))->setView(_view);

	connect(_userlist, &widgets::UserList::opCommand, m_doc->client(), &net::Client::sendMessage);
	connect(_dock_layers, &docks::LayerList::layerCommand, m_doc->client(), &net::Client::sendMessage);

	// Tool controller <-> UI connections
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged, _canvasscene, &drawingboard::CanvasScene::activeAnnotationChanged);

	connect(_dock_input, &docks::InputSettings::smoothingChanged, m_doc->toolCtrl(), &tools::ToolController::setSmoothing);
	m_doc->toolCtrl()->setSmoothing(_dock_input->getSmoothing());
	connect(m_doc->toolCtrl(), &tools::ToolController::toolCursorChanged, _view, &widgets::CanvasView::setToolCursor);
	_view->setToolCursor(m_doc->toolCtrl()->activeToolCursor());

	connect(_view, &widgets::CanvasView::penDown, m_doc->toolCtrl(), &tools::ToolController::startDrawing);
	connect(_view, &widgets::CanvasView::penMove, m_doc->toolCtrl(), &tools::ToolController::continueDrawing);
	connect(_view, &widgets::CanvasView::penHover, m_doc->toolCtrl(), &tools::ToolController::hoverDrawing);
	connect(_view, &widgets::CanvasView::penUp, m_doc->toolCtrl(), &tools::ToolController::endDrawing);
	connect(_view, &widgets::CanvasView::quickAdjust, _dock_toolsettings, &docks::ToolSettings::quickAdjustCurrent1);

	connect(_dock_layers, &docks::LayerList::layerSelected, m_doc->toolCtrl(), &tools::ToolController::setActiveLayer);
	connect(m_doc->toolCtrl(), &tools::ToolController::activeAnnotationChanged,
			static_cast<tools::AnnotationSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::ANNOTATION)), &tools::AnnotationSettings::setSelectionId);

	// Client command receive signals

	connect(m_doc->client(), SIGNAL(sentColorChange(QColor)), _dock_colors, SLOT(addLastUsedColor(QColor)));

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
	connect(m_doc, &Document::serverSpaceLowChanged, m_netstatus, &widgets::NetStatus::setLowSpaceAlert);
	connect(m_doc, &Document::sessionRoomcodeChanged, m_netstatus, &widgets::NetStatus::setRoomcode);

	connect(m_doc->client(), SIGNAL(bytesReceived(int)), m_netstatus, SLOT(bytesReceived(int)));
	connect(m_doc->client(), &net::Client::bytesSent, m_netstatus, &widgets::NetStatus::bytesSent);
	connect(m_doc->client(), &net::Client::lagMeasured, m_netstatus, &widgets::NetStatus::lagMeasured);
	connect(m_doc->client(), &net::Client::youWereKicked, m_netstatus, &widgets::NetStatus::kicked);
	connect(m_doc->client(), &net::Client::youWereKicked, m_chatbox, &widgets::ChatBox::kicked);

	connect(m_doc, &Document::sessionPreserveChatChanged, m_chatbox, &widgets::ChatBox::setPreserveMode);

	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateShortcuts()));
	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateTabletSupportMode()));

	updateTabletSupportMode();

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
#endif

	// Show self
	show();
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_MAC
	MacMenu::instance()->removeWindow(this);
#endif

	// Close playback dialog explicitly since it adds the miniplayer as a direct child
	// of the main window, but deletes it itself.
	delete m_playbackDialog;

	// Make sure all child dialogs are closed
	QObjectList lst = children();
	for(QObject *obj : lst) {
		QDialog *child = qobject_cast<QDialog*>(obj);
		delete child;
	}
}

void MainWindow::onCanvasChanged(canvas::CanvasModel *canvas)
{
	_canvasscene->initCanvas(canvas);

	connect(canvas->aclFilter(), &canvas::AclFilter::localOpChanged, this, &MainWindow::onOperatorModeChange);
	connect(canvas->aclFilter(), &canvas::AclFilter::localLockChanged, this, &MainWindow::updateLockWidget);
	connect(canvas->aclFilter(), &canvas::AclFilter::imageCmdLockChanged, this, &MainWindow::onImageCmdLockChange);

	connect(canvas, &canvas::CanvasModel::chatMessageReceived, m_chatbox, &widgets::ChatBox::receiveMessage);
	connect(canvas, &canvas::CanvasModel::chatMessageReceived, this, [this]() {
		// Show a "new message" indicator when the chatbox is collapsed
		if(_splitter->sizes().at(1)==0)
			_statusChatButton->show();
	});

	connect(canvas, &canvas::CanvasModel::markerMessageReceived, m_chatbox, &widgets::ChatBox::receiveMarker);

	connect(canvas, &canvas::CanvasModel::layerAutoselectRequest, _dock_layers, &docks::LayerList::selectLayer);
	connect(canvas, &canvas::CanvasModel::colorPicked, _dock_toolsettings, &docks::ToolSettings::setForegroundColor);
	connect(canvas, &canvas::CanvasModel::colorPicked, static_cast<tools::ColorPickerSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::PICKER)), &tools::ColorPickerSettings::addColor);

	connect(canvas, &canvas::CanvasModel::selectionRemoved, this, &MainWindow::selectionRemoved);

	connect(canvas, &canvas::CanvasModel::userJoined, m_netstatus, &widgets::NetStatus::join);
	connect(canvas, &canvas::CanvasModel::userLeft, m_netstatus, &widgets::NetStatus::leave);
	connect(canvas, &canvas::CanvasModel::userJoined, m_chatbox, &widgets::ChatBox::userJoined);
	connect(canvas, &canvas::CanvasModel::userLeft, m_chatbox, &widgets::ChatBox::userParted);

	connect(_dock_layers, &docks::LayerList::layerViewModeSelected, canvas, &canvas::CanvasModel::setLayerViewMode);

	_dock_layers->setCanvas(canvas);
	_userlist->setCanvas(canvas);

	_currentdoctools->setEnabled(true);
}

/**
 * @brief Initialize session state
 *
 * If the document in this window cannot be replaced, a new mainwindow is created.
 *
 * @return the MainWindow instance in which the document was loaded or 0 in case of error
 */
MainWindow *MainWindow::loadDocument(canvas::SessionLoader &loader)
{
	if(!canReplace()) {
		writeSettings();
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		if(!win->loadDocument(loader)) {
			// Whoops, this will delete the error dialog too. Show it again,
			// parented to current window
			showErrorMessage(loader.errorMessage());
			delete win;
			win = nullptr;
		}
		return win;
	}

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	if(!m_doc->loadCanvas(loader)) {
		QApplication::restoreOverrideCursor();
		showErrorMessage(loader.errorMessage());
		return nullptr;
	}

	if(!loader.warningMessage().isEmpty()) {
		QMessageBox::warning(0, QApplication::tr("Warning"), loader.warningMessage());
	}

	QApplication::restoreOverrideCursor();

	_currentdoctools->setEnabled(true);
	m_docadmintools->setEnabled(true);
	setDrawingToolsEnabled(true);
	getAction("hostsession")->setEnabled(true);

	return this;
}

MainWindow *MainWindow::loadRecording(recording::Reader *reader)
{
	if(!canReplace()) {
		writeSettings();
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		return win->loadRecording(reader);
	}

	m_doc->initCanvas();

	m_doc->canvas()->stateTracker()->setShowAllUserMarkers(true);

	_currentdoctools->setEnabled(true);
	m_docadmintools->setEnabled(true);
	setDrawingToolsEnabled(true);

	QFileInfo fileinfo(reader->filename());

	m_playbackDialog = new dialogs::PlaybackDialog(m_doc->canvas(), reader, this);
	m_playbackDialog->setWindowTitle(fileinfo.baseName() + " - " + m_playbackDialog->windowTitle());
	m_playbackDialog->setAttribute(Qt::WA_DeleteOnClose);

	connect(m_playbackDialog, SIGNAL(playbackToggled(bool)), this, SLOT(setRecorderStatus(bool))); // note: the argument goes unused in this case
	connect(m_playbackDialog, &dialogs::PlaybackDialog::destroyed, this, [this]() {
		m_playbackDialog = nullptr;
		getAction("recordsession")->setEnabled(true);
		setRecorderStatus(false);
		m_doc->canvas()->endPlayback();
	});

	m_playbackDialog->show();
	m_playbackDialog->centerOnParent();

	getAction("recordsession")->setEnabled(false);
	setRecorderStatus(false);

	return this;
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
			RecentFiles::initMenu(win->_recent);
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

	QList<QAction*> actions = findChildren<QAction*>();
	for(QAction *a : actions) {
		const QString &name = a->objectName();
		if(!name.isEmpty()) {
			if(cfg.contains(name))
				a->setShortcut(cfg.value(name).value<QKeySequence>());
			else if(CustomShortcutModel::hasDefaultShortcut(name))
				a->setShortcut(CustomShortcutModel::getDefaultShortcut(name));
		}
	}
}

/**
 * Reload shortcuts after they have been changed, for all open main windows
 */
void MainWindow::updateShortcuts()
{
	for(QWidget *widget : QApplication::topLevelWidgets()) {
		MainWindow *win = qobject_cast<MainWindow*>(widget);
		if(win) {
			win->loadShortcuts();
		}
	}
}

void MainWindow::updateTabletSupportMode()
{
	QSettings cfg;
	cfg.beginGroup("settings/input");

	const bool enable = cfg.value("tabletevents", true).toBool();
	const bool bugs = cfg.value("tabletbugs", false).toBool();
	const bool eraser = cfg.value("tableteraser", true).toBool();

	widgets::CanvasView::TabletMode mode;
	if(!enable)
		mode = widgets::CanvasView::DISABLE_TABLET;
	else if(bugs)
		mode = widgets::CanvasView::HYBRID_TABLET;
	else
		mode = widgets::CanvasView::ENABLE_TABLET;

	_view->setTabletMode(mode);

	// Handle eraser event
	if(eraser)
		connect(qApp, SIGNAL(eraserNear(bool)), _dock_toolsettings, SLOT(eraserNear(bool)), Qt::UniqueConnection);
	else
		disconnect(qApp, SIGNAL(eraserNear(bool)), _dock_toolsettings, SLOT(eraserNear(bool)));

	// not really tablet related, but close enough
	_view->setTouchGestures(
		cfg.value("touchscroll", true).toBool(),
		cfg.value("touchpinch", true).toBool(),
		cfg.value("touchtwist", true).toBool()
	);
	cfg.endGroup();
	_view->setBrushCursorStyle(cfg.value("settings/brushcursor").toInt());
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
		if(qApp->desktop()->availableGeometry().contains(pos))
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
		_splitter->restoreState(cfg.value("viewstate").toByteArray());
	}
	getAction("freezedocks")->setChecked(cfg.value("freezedocks", false).toBool());

	// Restore view settings
	bool pixelgrid = cfg.value("showgrid", true).toBool();
	getAction("showgrid")->setChecked(pixelgrid);
	_view->setPixelGrid(pixelgrid);

	cfg.endGroup();

	// Restore tool settings
	_dock_toolsettings->readSettings();

	// Customize shortcuts
	loadShortcuts();

	// Restore recent files
	RecentFiles::initMenu(_recent);
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
	cfg.setValue("viewstate", _splitter->saveState());
	cfg.setValue("freezedocks", getAction("freezedocks")->isChecked());

	cfg.setValue("showgrid", getAction("showgrid")->isChecked());
	cfg.endGroup();
	_dock_toolsettings->saveSettings();
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
		if(event->type() == QEvent::KeyRelease && _toolChangeTime.elapsed() > 250) {
			const QKeyEvent *e = static_cast<const QKeyEvent*>(event);
			if(!e->isAutoRepeat()) {
				if(_tempToolSwitchShortcut->isShortcutSent()) {
					if(e->modifiers() == Qt::NoModifier) {
						// Return from temporary tool change
						for(const QAction *act : m_drawingtools->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && e->key() == seq[0]) {
								_dock_toolsettings->setPreviousTool();
								break;
							}
						}

						// Return from temporary tool slot change
						for(const QAction *act : m_brushSlots->actions()) {
							const QKeySequence &seq = act->shortcut();
							if(seq.count()==1 && e->key() == seq[0]) {
								_dock_toolsettings->setPreviousTool();
								break;
							}
						}
					}
				}

				_tempToolSwitchShortcut->reset();
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
   canvas::BlankCanvasLoader bcl(size, background);
   loadDocument(bcl);
}

/**
 * Open the selected file
 * @param file file to open
 * @pre file.isEmpty()!=false
 */
void MainWindow::open(const QUrl& url)
{
	if(url.isLocalFile()) {
		QString file = url.toLocalFile();
		if(recording::Reader::isRecordingExtension(file)) {
			auto reader = dialogs::PlaybackDialog::openRecording(file, this);
			if(reader) {
				if(loadRecording(reader))
					addRecentFile(file);
				else
					delete reader;
			}
		} else {
			canvas::ImageCanvasLoader icl(file);
			if(loadDocument(icl))
				addRecentFile(file);
		}
	} else {
		networkaccess::getFile(url, QString(), m_netstatus, [this](const QFile &file, const QString &error) {
			if(error.isEmpty()) {
				open(QUrl::fromLocalFile(file.fileName()));
			} else {
				showErrorMessage(error);
			}
		});
	}
}

/**
 * Show a file selector dialog. If there are unsaved changes, open the file
 * in a new window.
 */
void MainWindow::open()
{
	// Get a list of supported formats
	QString dpimages = "*.ora ";
	QString dprecs = "*.dptxt *.dprec *.dprecz *.dprec.gz *.dptxtz *.dptxt.gz ";
	QString formats;
	for(QByteArray format : QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	const QString filter =
			tr("All Supported Files (%1)").arg(dpimages + dprecs + formats) + ";;" +
			tr("Images (%1)").arg(dpimages + formats) + ";;" +
			tr("Recordings (%1)").arg(dprecs) + ";;" +
			QApplication::tr("All Files (*)");

	// Get the file name to open
	const QUrl file = QUrl::fromLocalFile(QFileDialog::getOpenFileName(this,
			tr("Open Image"), getLastPath(), filter));

	// Open the file if it was selected
	if(file.isValid()) {
		setLastPath(file.toString());
		open(file);
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

	if(filename.isEmpty()) {
		saveas();
		return;
	}

	if(!utils::isWritableFormat(filename)) {
		saveas();
		return;
	}

	if(!filename.endsWith("ora", Qt::CaseInsensitive) && m_doc->canvas()->needsOpenRaster()) {
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
	QStringList filter;

	// Get a list of all supported formats
	for(const QPair<QString,QByteArray> &format : utils::writableImageFormats()) {
		filter << format.first + " (*." + format.second + ")";
	}
	filter << QApplication::tr("All Files (*)");

	// Get the file name
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save Image"), getLastPath(), filter.join(";;"), &selfilter);
	if(file.isEmpty()==false) {

		// Set file suffix if missing
		const QFileInfo info(file);
		if(info.suffix().isEmpty()) {
			if(selfilter.isEmpty()) {
				// If we don't have selfilter, pick what is best
				if(m_doc->canvas()->needsOpenRaster())
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
		if(m_doc->canvas()->needsOpenRaster() && !file.endsWith(".ora", Qt::CaseInsensitive)) {
			if(confirmFlatten(file)==false)
				return;
		}

		// Save the image
		m_doc->saveCanvas(file);
		addRecentFile(file);
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
		showErrorMessage(tr("Couldn't save image"), errorMessage);
	else
		m_viewStatusBar->showMessage(tr("Image saved"), 1000);

	// Cancel exit if canvas is modified while it was being saved
	if(m_doc->isDirty())
		m_exitAfterSave = false;

	if(m_exitAfterSave)
		close();
}

void MainWindow::exportAnimation()
{
	auto *dlg = new dialogs::VideoExportDialog(this);
	dlg->showAnimationSettings(m_doc->canvas()->layerStack()->layerCount());

	connect(dlg, &QDialog::finished, this, [dlg, this](int result) {
		if(result == QDialog::Accepted) {
			VideoExporter *vexp = dlg->getExporter();
			if(vexp) {
				auto *exporter = new AnimationExporter(m_doc->canvas()->layerStack(), vexp, this);
				vexp->setParent(exporter);

				connect(exporter, &AnimationExporter::done, exporter, &AnimationExporter::deleteLater);
				connect(exporter, &AnimationExporter::error, this, [this](const QString &msg) {
						QMessageBox::warning(this, tr("Export error"), msg);
				});

				exporter->setStartFrame(dlg->getFirstLayer());
				exporter->setEndFrame(dlg->getLastLayer());
				exporter->setUseBgLayer(dlg->useBackgroundLayer());
				exporter->setBackground(dlg->animationBackground());

				auto *pdlg = new QProgressDialog(tr("Exporting..."), tr("Cancel"), 1, dlg->getLastLayer(), this);
				pdlg->setWindowModality(Qt::WindowModal);
				pdlg->setAttribute(Qt::WA_DeleteOnClose);

				connect(exporter, &AnimationExporter::progress, pdlg, &QProgressDialog::setValue);
				connect(exporter, &AnimationExporter::done, pdlg, &QProgressDialog::close);

				pdlg->show();
				exporter->start();
			}
		}
		dlg->deleteLater();
	});

	dlg->show();
}

void MainWindow::showFlipbook()
{
	dialogs::Flipbook *fp = new dialogs::Flipbook(this);
	fp->setAttribute(Qt::WA_DeleteOnClose);
	fp->setLayers(m_doc->canvas()->layerStack());
	fp->show();
}

void MainWindow::setRecorderStatus(bool on)
{
	if(m_playbackDialog) {
		if(m_playbackDialog->isPlaying()) {
			_recorderstatus->setPixmap(icon::fromTheme("media-playback-start").pixmap(16, 16));
			_recorderstatus->setToolTip("Playing back recording");
		} else {
			_recorderstatus->setPixmap(icon::fromTheme("media-playback-pause").pixmap(16, 16));
			_recorderstatus->setToolTip("Playback paused");
		}
		_recorderstatus->show();
	} else {
		QAction *recordAction = getAction("recordsession");
		if(on) {
			QIcon icon = icon::fromTheme("media-record");
			_recorderstatus->setPixmap(icon.pixmap(16, 16));
			_recorderstatus->setToolTip("Recording session");
			_recorderstatus->show();
			recordAction->setText(tr("Stop Recording"));
			recordAction->setIcon(icon::fromTheme("media-playback-stop"));
		} else {

			_recorderstatus->hide();
			recordAction->setText(tr("Record..."));
			recordAction->setIcon(icon::fromTheme("media-record"));
		}

		getAction("toolmarker")->setEnabled(on);
	}
}

void MainWindow::toggleRecording()
{
	if(m_doc->isRecording()) {
		m_doc->stopRecording();
		return;
	}

	QString filter =
			tr("Binary Recordings (%1)").arg("*.dprec") + ";;" +
			tr("Text Recordings (%1)").arg("*.dptxt") + ";;" +
			tr("Compressed Binary Recordings (%1)").arg("*.dprecz") + ";;" +
			tr("Compressed Text Recordings (%1)").arg("*.dptxtz") + ";;" +
			QApplication::tr("All Files (*)");
	QString file = QFileDialog::getSaveFileName(this,
			tr("Record Session"), getLastPath(), filter);

	if(!file.isEmpty()) {
		QString error;
		if(!m_doc->startRecording(file, &error))
			showErrorMessage(error);
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
	const bool useremote = dlg->useRemoteAddress();
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
	address.setUserName(dlg->getUserName());

	// Start server if hosting locally
	if(useremote==false) {
		net::ServerThread *server = new net::ServerThread;
		server->setDeleteOnExit();

		int port = server->startServer(dlg->getTitle());
		if(!port) {
			QMessageBox::warning(this, tr("Host Session"), server->errorString());
			dlg->show();
			delete server;
			return;
		}

		if(!server->isOnDefaultPort())
			address.setPort(port);
	}

	// Connect to server
	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::HOST, address, this);
	login->setSessionAlias(dlg->getSessionAlias());
	login->setPassword(dlg->getPassword());
	login->setTitle(dlg->getTitle());
	login->setMaxUsers(dlg->getUserLimit());
	login->setAllowDrawing(dlg->getAllowDrawing());
	login->setLayerControlLock(dlg->getLayerControlLock());
	login->setPreserveChat(dlg->getPreserveChat());
	login->setAnnounceUrl(dlg->getAnnouncementUrl(), dlg->getAnnouncmentPrivate());
	login->setInitialState(m_doc->canvas()->generateSnapshot(true));
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

			joinSession(url, dlg->recordSession());
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
	const int usercount = userlist->rowCount();
	for(int i=0;i<usercount;++i) {
		const canvas::User &u = userlist->data(userlist->index(i)).value<canvas::User>();
		if(u.id != m_doc->canvas()->localUserId())
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
	auto dlg = new dialogs::ResetDialog(m_doc->canvas()->stateTracker(), this);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	// Automatically lock the session while we are preparing to reset
	bool wasLocked = m_doc->canvas()->aclFilter()->isSessionLocked();
	if(!wasLocked) {
		m_doc->sendLockSession(true);
	}

	connect(dlg, &dialogs::ResetDialog::accepted, this, [this, dlg]() {
		// Send request for reset. No need to unlock the session,
		// since the reset itself will do that for us.
		m_doc->sendResetSession(dlg->selectedSavepoint());
	});

	connect(dlg, &dialogs::ResetDialog::rejected, this, [this, wasLocked]() {
		// Reset cancelled: unlock the session (if locked by as)
		if(!wasLocked)
			m_doc->sendLockSession(false);
	});

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
void MainWindow::joinSession(const QUrl& url, bool autoRecord)
{
	if(!canReplace()) {
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		win->joinSession(url, autoRecord);
		return;
	}

	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::JOIN, url, this);
	auto *dlg = new dialogs::LoginDialog(login, this);
	connect(m_doc, &Document::catchupProgress, dlg, &dialogs::LoginDialog::catchupProgress);
	connect(m_doc, &Document::serverLoggedIn, dlg, [dlg,this](bool join) {
		dlg->onLoginDone(join);
		_canvasscene->hideCanvas();
	});
	connect(dlg, &dialogs::LoginDialog::destroyed, _canvasscene, &drawingboard::CanvasScene::showCanvas);

	dlg->show();
	m_doc->setAutoRecordOnConnect(autoRecord);
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
	_view->setEnabled(false);
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
	m_docadmintools->setEnabled(true);
	m_sessionSettings->close();

	// Re-enable UI
	_view->setEnabled(true);
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
		QTimer::singleShot(1, msgbox, &QMessageBox::show);
	}
}

/**
 * Server connection established and login successfull
 */
void MainWindow::onServerLogin()
{
	m_netstatus->loggedIn(m_doc->client()->sessionUrl());
	m_netstatus->setSecurityLevel(m_doc->client()->securityLevel(), m_doc->client()->hostCertificate());
	m_chatbox->setLocalUserId(m_doc->client()->myId());
	_view->setEnabled(true);
	m_sessionSettings->setPersistenceEnabled(m_doc->client()->serverSuppotsPersistence());
	setDrawingToolsEnabled(true);
	m_modtools->setEnabled(m_doc->client()->isModerator());
	getAction("reportabuse")->setEnabled(m_doc->client()->serverSupportsReports());
}

void MainWindow::updateLockWidget()
{
	bool locked = m_doc->canvas() && m_doc->canvas()->aclFilter()->isSessionLocked();
	getAction("locksession")->setChecked(locked);

	locked |= (m_doc->canvas() && m_doc->canvas()->aclFilter()->isLocked()) || _dock_layers->isCurrentLayerLocked();

	if(locked) {
		_lockstatus->setPixmap(icon::fromTheme("object-locked").pixmap(16, 16));
		_lockstatus->setToolTip(tr("Board is locked"));
	} else {
		_lockstatus->setPixmap(QPixmap());
		_lockstatus->setToolTip(QString());
	}
	_view->setLocked(locked);
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
	m_docadmintools->setEnabled(op);
	_dock_layers->setOperatorMode(op);
	onImageCmdLockChange(m_doc->canvas()->aclFilter()->isImagesLocked());
	getAction("gainop")->setEnabled(!op && m_doc->isSessionOpword());
}

void MainWindow::onImageCmdLockChange(bool lock)
{
	const bool e = !lock || m_doc->canvas()->aclFilter()->isLocalUserOperator();

	static const char *IMAGE_ACTIONS[] = {
		"cutlayer", "paste", "pastefile", "stamp",
		"cleararea", "fillfgarea", "recolorarea", "colorerasearea",
		"toolfill"
	};
	for(const char *a : IMAGE_ACTIONS)
		getAction(a)->setEnabled(e);
}

/**
 * Write settings and exit. The application will not be terminated until
 * the last mainwindow is closed.
 */
void MainWindow::exit()
{
	if(windowState().testFlag(Qt::WindowFullScreen))
		toggleFullscreen();
	writeSettings();
	deleteLater();
}

/**
 * @param message error message
 * @param details error details
 */
void MainWindow::showErrorMessage(const QString& message, const QString& details)
{
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

void MainWindow::setShowAnnotations(bool show)
{
	QAction *annotationtool = getAction("tooltext");
	annotationtool->setEnabled(show);
	_canvasscene->showAnnotations(show);
	if(!show) {
		if(annotationtool->isChecked())
			getAction("toolbrush")->trigger();
	}
}

void MainWindow::setShowLaserTrails(bool show)
{
	QAction *lasertool = getAction("toollaser");
	lasertool->setEnabled(show);
	_canvasscene->showLaserTrails(show);
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
		m_fullscreenOldState = saveState();
		m_fullscreenOldGeometry = geometry();
		m_fullscreenOldMaximized = isMaximized();

		// Hide everything except floating docks
		menuBar()->hide();
		m_viewStatusBar->hide();
		_view->setFrameShape(QFrame::NoFrame);
		for(QObject *child : children()) {
			if(child->inherits("QDockWidget")) {
				QDockWidget *dw = qobject_cast<QDockWidget*>(child);
				if(!dw->isFloating())
					dw->hide();
			} else if(child->inherits("QToolBar"))
				(qobject_cast<QWidget*>(child))->hide();
		}

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
		m_viewStatusBar->show();
		_view->setFrameShape(QFrame::StyledPanel);
		restoreState(m_fullscreenOldState);
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

	if(_dock_toolsettings->currentTool() == idx) {
		if(QSettings().value("settings/tooltoggle", true).toBool())
			_dock_toolsettings->setPreviousTool();
		_tempToolSwitchShortcut->reset();

	} else {
		_dock_toolsettings->setTool(tools::Tool::Type(idx));
		_toolChangeTime.start();
		_lastToolBeforePaste = -1;
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
	_canvasscene->showAnnotationBorders(tool==tools::Tool::ANNOTATION);

	// Send pointer updates when using the laser pointer (TODO checkbox)
	_view->setPointerTracking(tool==tools::Tool::LASERPOINTER && static_cast<tools::LaserPointerSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::LASERPOINTER))->pointerTracking());

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
	if(_lastToolBeforePaste>=0) {
		// Selection was just removed and we had just pasted an image
		// so restore the previously used tool
		QAction *toolaction = m_drawingtools->actions().at(_lastToolBeforePaste);
		toolaction->trigger();
	}
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
			if(pos.size() == 2) {
				bool ok1, ok2;
				pastepos = QPoint(pos.at(0).toInt(&ok1), pos.at(1).toInt(&ok2));
				pasteAtPos = ok1 && ok2;
			}
		}

		// Paste-in-place if source was Drawpile (and source is visible)
		if(pasteAtPos && _view->isPointVisible(pastepos))
			pasteImage(data->imageData().value<QImage>(), &pastepos);
		else
			pasteImage(data->imageData().value<QImage>());
	}
}

void MainWindow::pasteFile()
{
	// Get a list of supported formats
	QString formats;
	for(QByteArray format : QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	const QString filter = tr("Images (%1)").arg(formats) + ";;" + QApplication::tr("All Files (*)");

	// Get the file name to open
	const QString file = QFileDialog::getOpenFileName(this,
			tr("Paste Image"), getLastPath(), filter);

	// Open the file if it was selected
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
		networkaccess::getImage(url, m_netstatus, [this](const QImage &image, const QString &error) {
			if(image.isNull())
				showErrorMessage(error);
			else
				pasteImage(image);
		});
	}
}

void MainWindow::pasteImage(const QImage &image, const QPoint *point)
{
	if(_dock_toolsettings->currentTool() != tools::Tool::SELECTION && _dock_toolsettings->currentTool() != tools::Tool::POLYGONSELECTION) {
		int currentTool = _dock_toolsettings->currentTool();
		getAction("toolselectrect")->trigger();
		_lastToolBeforePaste = currentTool;
	}

	QPoint p;
	bool force;
	if(point) {
		p = *point;
		force = true;
	} else {
		p = _view->viewCenterPoint();
		force = false;
	}

	m_doc->pasteImage(image, p, force);
}

void MainWindow::dropUrl(const QUrl &url)
{
	if(url.isLocalFile()) {
		// Is this an image file?
		QImage img(url.toLocalFile());
		if(img.isNull()) {
			// Not a simple image, try opening it as a document
			open(url);

		} else {
			pasteImage(img);
		}

	} else {
		networkaccess::getFile(url, "", m_netstatus, [this](const QFile &file, const QString &error) {
			if(error.isEmpty())
				dropUrl(QUrl::fromLocalFile(file.fileName()));
			else
				showErrorMessage(error);
		});
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
		const int a = static_cast<tools::AnnotationSettings*>(_dock_toolsettings->getToolSettingsPage(tools::Tool::ANNOTATION))->selected();
		if(a>0) {
			QList<protocol::MessagePtr> msgs;
			msgs << protocol::MessagePtr(new protocol::UndoPoint(m_doc->client()->myId()));
			msgs << protocol::MessagePtr(new protocol::AnnotationDelete(m_doc->client()->myId(), a));
			m_doc->client()->sendMessages(msgs);
			return;
		}
	}

	// No annotation selected: clear seleted area as usual
	m_doc->fillArea(Qt::white, paintcore::BlendMode::MODE_ERASE);
}

void MainWindow::resizeCanvas()
{
	if(!m_doc->canvas()) {
		qWarning("resizeCanvas: no canvas!");
		return;
	}

	QSize size = m_doc->canvas()->layerStack()->size();
	dialogs::ResizeDialog *dlg = new dialogs::ResizeDialog(size, this);
	dlg->setPreviewImage(m_doc->canvas()->toImage().scaled(300, 300, Qt::KeepAspectRatio));
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	connect(dlg, &QDialog::accepted, this, [this, dlg]() {
		dialogs::ResizeVector r = dlg->resizeVector();
		if(!r.isZero()) {
			m_doc->sendResizeCanvas(r.top, r.right, r.bottom, r.left);
		}
	});
	dlg->show();
}

void MainWindow::markSpotForRecording()
{
	bool ok;
	QString text = QInputDialog::getText(this, tr("Mark"), tr("Marker text"), QLineEdit::Normal, QString(), &ok);
	if(ok) {
		m_doc->client()->sendMessage(protocol::MessagePtr(new protocol::Marker(0, text)));
	}
}

void MainWindow::about()
{
	QMessageBox::about(0, tr("About Drawpile"),
			QStringLiteral("<p><b>Drawpile %1</b><br>").arg(DRAWPILE_VERSION) +
			tr("A collaborative drawing program.") + QStringLiteral("</p>"

			"<p>Copyright © 2006-2017 Calle Laakkonen</p>"

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
			)
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
 * @param name (internal) name of the action. If null, no name is set. If no name is set, the shortcut cannot be customized.
 * @param icon name of the icon file to use. If 0, no icon is set.
 * @param text action text
 * @param tip status bar tip
 * @param shortcut default shortcut
 * @param checkable is this a checkable action
 */
QAction *MainWindow::makeAction(const char *name, const char *icon, const QString& text, const QString& tip, const QKeySequence& shortcut, bool checkable)
{
	QAction *act;
	QIcon qicon;
	if(icon)
		qicon = icon::fromTheme(icon);
	act = new QAction(qicon, text, this);
	if(name)
		act->setObjectName(name);
	if(shortcut.isEmpty()==false)
		act->setShortcut(shortcut);

	act->setCheckable(checkable);

	act->setAutoRepeat(false);

	if(tip.isEmpty()==false)
		act->setStatusTip(tip);

	if(name!=0 && name[0]!='\0')
		CustomShortcutModel::registerCustomizableAction(act->objectName(), act->text().remove('&'), shortcut);

	// Add this action to the mainwindow so its shortcut can be used
	// even when the menu/toolbar is not visible
	addAction(act);

	return act;
}

QAction *MainWindow::getAction(const QString &name)
{
	QAction *a = findChild<QAction*>(name, Qt::FindDirectChildrenOnly);
	if(!a)
		qFatal("%s: no such action", name.toLocal8Bit().constData());
	Q_ASSERT(a);
	return a;
}

/**
 * @brief Create actions, menus and toolbars
 */
void MainWindow::setupActions()
{
	// Action groups
	_currentdoctools = new QActionGroup(this);
	_currentdoctools->setExclusive(false);
	_currentdoctools->setEnabled(false);

	m_admintools = new QActionGroup(this);
	m_admintools->setExclusive(false);

	m_modtools = new QActionGroup(this);
	m_modtools->setEnabled(false);

	m_docadmintools = new QActionGroup(this);
	m_docadmintools->setExclusive(false);
	m_docadmintools->setEnabled(false);

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
	QAction *freezeDocks = makeAction("freezedocks", nullptr, tr("Lock in place"), QString(), QKeySequence(), true);
	toggledockmenu->addAction(freezeDocks);
	connect(freezeDocks, &QAction::toggled, this, &MainWindow::setFreezeDocks);

	//
	// File menu and toolbar
	//
	QAction *newdocument = makeAction("newdocument", "document-new", tr("&New"), QString(), QKeySequence::New);
	QAction *open = makeAction("opendocument", "document-open", tr("&Open..."), QString(), QKeySequence::Open);
#ifdef Q_OS_MAC
	QAction *closefile = makeAction("closedocument", 0, tr("Close"), QString(), QKeySequence::Close);
#endif
	QAction *save = makeAction("savedocument", "document-save",tr("&Save"), QString(),QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", "document-save-as", tr("Save &As..."), QString(), QKeySequence::SaveAs);
	QAction *autosave = makeAction("autosave", 0, tr("Autosave"), QString(), QKeySequence(), true);
	autosave->setEnabled(false);
	QAction *exportAnimation = makeAction("exportanim", 0, tr("&Animation..."));

	QAction *record = makeAction("recordsession", "media-record", tr("Record..."));
	QAction *quit = makeAction("exitprogram", "application-exit", tr("&Quit"), QString(), QKeySequence("Ctrl+Q"));
	quit->setMenuRole(QAction::QuitRole);

#ifdef Q_OS_MAC
	_currentdoctools->addAction(closefile);
#endif
	_currentdoctools->addAction(save);
	_currentdoctools->addAction(saveas);
	_currentdoctools->addAction(exportAnimation);
	_currentdoctools->addAction(record);

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));

	connect(autosave, &QAction::triggered, m_doc, &Document::setAutosave);
	connect(m_doc, &Document::autosaveChanged, autosave, &QAction::setChecked);
	connect(m_doc, &Document::canAutosaveChanged, autosave, &QAction::setEnabled);

	connect(exportAnimation, SIGNAL(triggered()), this, SLOT(exportAnimation()));
	connect(record, SIGNAL(triggered()), this, SLOT(toggleRecording()));
#ifdef Q_OS_MAC
	connect(closefile, SIGNAL(triggered()), this, SLOT(close()));
	connect(quit, SIGNAL(triggered()), MacMenu::instance(), SLOT(quitAll()));
#else
	connect(quit, SIGNAL(triggered()), this, SLOT(close()));
#endif

	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(newdocument);
	filemenu->addAction(open);
	_recent = filemenu->addMenu(tr("Open &Recent"));
	filemenu->addSeparator();

#ifdef Q_OS_MAC
	filemenu->addAction(closefile);
#endif
	filemenu->addAction(save);
	filemenu->addAction(saveas);
	filemenu->addAction(autosave);
	filemenu->addSeparator();

	QMenu *exportMenu = filemenu->addMenu(tr("&Export"));
	exportMenu->setIcon(icon::fromTheme("document-export"));
	exportMenu->addAction(exportAnimation);
	filemenu->addAction(record);
	filemenu->addSeparator();

	filemenu->addAction(quit);

	QToolBar *filetools = new QToolBar(tr("File Tools"));
	filetools->setObjectName("filetoolsbar");
	toggletoolbarmenu->addAction(filetools->toggleViewAction());
	filetools->addAction(newdocument);
	filetools->addAction(open);
	filetools->addAction(save);
	addToolBar(Qt::TopToolBarArea, filetools);

	connect(_recent, &QMenu::triggered, this, [this](QAction *action) {
		this->open(QUrl::fromLocalFile(action->property("filepath").toString()));
	});

	//
	// Edit menu
	//
	QAction *undo = makeAction("undo", "edit-undo", tr("&Undo"), QString(), QKeySequence::Undo);
	QAction *redo = makeAction("redo", "edit-redo", tr("&Redo"), QString(), QKeySequence::Redo);
	QAction *copy = makeAction("copyvisible", "edit-copy", tr("&Copy Visible"), tr("Copy selected area to the clipboard"), QKeySequence("Shift+Ctrl+C"));
	QAction *copylayer = makeAction("copylayer", "edit-copy", tr("Copy &Layer"), tr("Copy selected area of the current layer to the clipboard"), QKeySequence::Copy);
	QAction *cutlayer = makeAction("cutlayer", "edit-cut", tr("Cu&t Layer"), tr("Cut selected area of the current layer to the clipboard"), QKeySequence::Cut);
	QAction *paste = makeAction("paste", "edit-paste", tr("&Paste"), QString(), QKeySequence::Paste);
	QAction *stamp = makeAction("stamp", 0, tr("&Stamp"), QString(), QKeySequence("Ctrl+T"));

	QAction *pastefile = makeAction("pastefile", "document-open", tr("Paste &From File..."));
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", 0, tr("Delete Empty Annotations"));
	QAction *resize = makeAction("resizecanvas", 0, tr("Resi&ze Canvas..."));
	QAction *preferences = makeAction(0, 0, tr("Prefere&nces")); preferences->setMenuRole(QAction::PreferencesRole);

	QAction *selectall = makeAction("selectall", 0, tr("Select &All"), QString(), QKeySequence::SelectAll);
#if (defined(Q_OS_MAC) || defined(Q_OS_WIN)) // Deselect is not defined on Mac and Win
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"), QString(), QKeySequence("Shift+Ctrl+A"));
#else
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"), QString(), QKeySequence::Deselect);
#endif

	QAction *expandup = makeAction("expandup", 0, tr("Expand &Up"), "", QKeySequence(CTRL_KEY "+J"));
	QAction *expanddown = makeAction("expanddown", 0, tr("Expand &Down"), "", QKeySequence(CTRL_KEY "+K"));
	QAction *expandleft = makeAction("expandleft", 0, tr("Expand &Left"), "", QKeySequence(CTRL_KEY "+H"));
	QAction *expandright = makeAction("expandright", 0, tr("Expand &Right"), "", QKeySequence(CTRL_KEY "+L"));

	QAction *cleararea = makeAction("cleararea", 0, tr("Delete"), QString(), QKeySequence("Delete"));
	QAction *fillfgarea = makeAction("fillfgarea", 0, tr("Fill Selection"), QString(), QKeySequence(CTRL_KEY "+,"));
	QAction *recolorarea = makeAction("recolorarea", 0, tr("Recolor Selection"), QString(), QKeySequence(CTRL_KEY "+Shift+,"));
	QAction *colorerasearea = makeAction("colorerasearea", 0, tr("Color Erase Selection"), QString(), QKeySequence("Shift+Delete"));

	_currentdoctools->addAction(undo);
	_currentdoctools->addAction(redo);
	_currentdoctools->addAction(copy);
	_currentdoctools->addAction(copylayer);
	_currentdoctools->addAction(cutlayer);
	_currentdoctools->addAction(stamp);
	_currentdoctools->addAction(deleteAnnotations);
	_currentdoctools->addAction(cleararea);
	_currentdoctools->addAction(fillfgarea);
	_currentdoctools->addAction(recolorarea);
	_currentdoctools->addAction(colorerasearea);
	_currentdoctools->addAction(selectall);
	_currentdoctools->addAction(selectnone);

	m_docadmintools->addAction(resize);
	m_docadmintools->addAction(expandup);
	m_docadmintools->addAction(expanddown);
	m_docadmintools->addAction(expandleft);
	m_docadmintools->addAction(expandright);

	connect(undo, &QAction::triggered, m_doc, &Document::undo);
	connect(redo, &QAction::triggered, m_doc, &Document::redo);
	connect(copy, &QAction::triggered, m_doc, &Document::copyVisible);
	connect(copylayer, &QAction::triggered, m_doc, &Document::copyLayer);
	connect(cutlayer, &QAction::triggered, m_doc, &Document::cutLayer);
	connect(paste, &QAction::triggered, this, &MainWindow::paste);
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
	connect(fillfgarea, &QAction::triggered, this, [this]() { m_doc->fillArea(_dock_toolsettings->foregroundColor(), paintcore::BlendMode::MODE_NORMAL); });
	connect(recolorarea, &QAction::triggered, this, [this]() { m_doc->fillArea(_dock_toolsettings->foregroundColor(), paintcore::BlendMode::MODE_RECOLOR); });
	connect(colorerasearea, &QAction::triggered, this, [this]() { m_doc->fillArea(_dock_toolsettings->foregroundColor(), paintcore::BlendMode::MODE_COLORERASE); });
	connect(resize, SIGNAL(triggered()), this, SLOT(resizeCanvas()));
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
	editmenu->addAction(copylayer);
	editmenu->addAction(paste);
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

	QAction *toggleChat = makeAction("togglechat", 0, tr("Chat"), QString(), QKeySequence("Alt+C"), true);

	QAction *showFlipbook = makeAction("showflipbook", 0, tr("Flipbook"), tr("Show animation preview window"), QKeySequence("Ctrl+F"));

	QAction *zoomin = makeAction("zoomin", "zoom-in",tr("Zoom &In"), QString(), QKeySequence::ZoomIn);
	QAction *zoomout = makeAction("zoomout", "zoom-out",tr("Zoom &Out"), QString(), QKeySequence::ZoomOut);
	QAction *zoomorig = makeAction("zoomone", "zoom-original",tr("&Normal Size"), QString(), QKeySequence(Qt::CTRL + Qt::Key_0));
	QAction *rotateorig = makeAction("rotatezero", "transform-rotate", tr("&Reset Rotation"), QString(), QKeySequence(Qt::CTRL + Qt::Key_R));
	QAction *rotatecw = makeAction("rotatecw", 0, tr("Rotate Clockwise"), QString(), QKeySequence(Qt::SHIFT + Qt::Key_Period));
	QAction *rotateccw = makeAction("rotateccw", 0, tr("Rotate Counterclockwise°"), QString(), QKeySequence(Qt::SHIFT + Qt::Key_Comma));

	QAction *rotate90 = makeAction("rotate90", 0, tr("Rotate to 90°"));
	QAction *rotate180 = makeAction("rotate180", 0, tr("Rotate to 180°"));
	QAction *rotate270 = makeAction("rotate270", 0, tr("Rotate to 270°"));

	QAction *viewmirror = makeAction("viewmirror", "object-flip-horizontal", tr("Mirror"), QString(), QKeySequence("V"), true);
	QAction *viewflip = makeAction("viewflip", "object-flip-vertical", tr("Flip"), QString(), QKeySequence("C"), true);

	QAction *showannotations = makeAction("showannotations", 0, tr("Show &Annotations"), QString(), QKeySequence(), true);
	QAction *showusermarkers = makeAction("showusermarkers", 0, tr("Show User &Pointers"), QString(), QKeySequence(), true);
	QAction *showuserlayers = makeAction("showuserlayers", 0, tr("Show User &Layers"), QString(), QKeySequence(), true);
	QAction *showlasers = makeAction("showlasers", 0, tr("Show La&ser Trails"), QString(), QKeySequence(), true);
	QAction *showgrid = makeAction("showgrid", 0, tr("Show Pixel &Grid"), QString(), QKeySequence(), true);
	toggleChat->setChecked(true);
	showannotations->setChecked(true);
	showusermarkers->setChecked(true);
	showuserlayers->setChecked(true);
	showlasers->setChecked(true);
	showgrid->setChecked(true);

	QAction *fullscreen = makeAction("fullscreen", 0, tr("&Full Screen"), QString(), QKeySequence::FullScreen, true);

	_currentdoctools->addAction(showFlipbook);

	if(windowHandle()) { // mainwindow should always be a native window, but better safe than sorry
		connect(windowHandle(), &QWindow::windowStateChanged, fullscreen, [fullscreen](Qt::WindowState state) {
			// Update the mode tickmark on fulscreen state change.
			// On Qt 5.3.0, this signal doesn't seem to get emitted on OSX when clicking
			// on the toggle button in the titlebar. The state can be queried correctly though.
			fullscreen->setChecked(state & Qt::WindowFullScreen);
		});
	}

	connect(_statusChatButton, &QToolButton::clicked, toggleChat, &QAction::trigger);

	connect(m_chatbox, &widgets::ChatBox::expanded, toggleChat, &QAction::setChecked);
	connect(m_chatbox, &widgets::ChatBox::expanded, _statusChatButton, &QToolButton::hide);
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
		_splitter->setSizes(sizes);
	});

	connect(showFlipbook, SIGNAL(triggered()), this, SLOT(showFlipbook()));

	connect(zoomin, SIGNAL(triggered()), _view, SLOT(zoomin()));
	connect(zoomout, SIGNAL(triggered()), _view, SLOT(zoomout()));
	connect(zoomorig, &QAction::triggered, this, [this]() { _view->setZoom(100.0); });
	connect(rotateorig, &QAction::triggered, this, [this]() { _view->setRotation(0); });
	connect(rotatecw, &QAction::triggered, this, [this]() { _view->setRotation(_view->rotation() + 5); });
	connect(rotateccw, &QAction::triggered, this, [this]() { _view->setRotation(_view->rotation() - 5); });
	connect(rotate90, &QAction::triggered, this, [this]() { _view->setRotation(90); });
	connect(rotate180, &QAction::triggered, this, [this]() { _view->setRotation(180); });
	connect(rotate270, &QAction::triggered, this, [this]() { _view->setRotation(270); });
	connect(viewflip, SIGNAL(triggered(bool)), _view, SLOT(setViewFlip(bool)));
	connect(viewmirror, SIGNAL(triggered(bool)), _view, SLOT(setViewMirror(bool)));

	connect(fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullscreen()));

	connect(showannotations, SIGNAL(triggered(bool)), this, SLOT(setShowAnnotations(bool)));
	connect(showusermarkers, SIGNAL(triggered(bool)), _canvasscene, SLOT(showUserMarkers(bool)));
	connect(showuserlayers, SIGNAL(triggered(bool)), _canvasscene, SLOT(showUserLayers(bool)));
	connect(showlasers, SIGNAL(triggered(bool)), this, SLOT(setShowLaserTrails(bool)));
	connect(showgrid, SIGNAL(triggered(bool)), _view, SLOT(setPixelGrid(bool)));

	_viewstatus->setZoomActions(zoomin, zoomout, zoomorig);
	_viewstatus->setRotationActions(rotateorig);
	_viewstatus->setFlipActions(viewflip, viewmirror);

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
	rotatemenu->addAction(rotate90);
	rotatemenu->addAction(rotate180);
	rotatemenu->addAction(rotate270);

	viewmenu->addAction(viewflip);
	viewmenu->addAction(viewmirror);

	viewmenu->addSeparator();

	QMenu *userpointermenu = viewmenu->addMenu(tr("User Pointers"));
	userpointermenu->addAction(showusermarkers);
	userpointermenu->addAction(showuserlayers);
	userpointermenu->addAction(showlasers);

	viewmenu->addAction(showannotations);

	viewmenu->addAction(showgrid);

	viewmenu->addSeparator();
	viewmenu->addAction(fullscreen);

	//
	// Session menu
	//
	QAction *host = makeAction("hostsession", 0, tr("&Host..."),tr("Share your drawingboard with others"));
	QAction *join = makeAction("joinsession", 0, tr("&Join..."),tr("Join another user's drawing session"));
	QAction *logout = makeAction("leavesession", 0, tr("&Leave"),tr("Leave this drawing session"));
	logout->setEnabled(false);

	QAction *serverlog = makeAction("viewserverlog", 0, tr("Event Log"), QString(), QKeySequence("F10"));
	QAction *sessionSettings = makeAction("sessionsettings", 0, tr("Settings..."), QString(), QKeySequence("F11"));
	sessionSettings->setMenuRole(QAction::NoRole); // Keep this item where it is on OSX
	sessionSettings->setEnabled(false);

	QAction *resetsession = makeAction("resetsession", 0, tr("&Reset..."));
	QAction *terminatesession = makeAction("terminatesession", 0, tr("Terminate"));

	QAction *reportabuse = makeAction("reportabuse", 0, tr("Report..."));
	reportabuse->setEnabled(false);

	QAction *gainop = makeAction("gainop", 0, tr("Become Operator..."));
	gainop->setEnabled(false);

	QAction *locksession = makeAction("locksession", 0, tr("Lock Everything"), tr("Prevent changes to the drawing board"), QKeySequence("F12"), true);

	m_admintools->addAction(locksession);
	m_admintools->addAction(resetsession);
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
		gainop->setEnabled(hasOpword && !m_doc->canvas()->aclFilter()->isLocalUserOperator());
	});

	connect(resetsession, &QAction::triggered, this, &MainWindow::resetSession);
	connect(terminatesession, &QAction::triggered, this, &MainWindow::terminateSession);

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host);
	sessionmenu->addAction(join);
	sessionmenu->addAction(logout);
	sessionmenu->addSeparator();

	QMenu *modmenu = sessionmenu->addMenu(tr("Moderation"));
	modmenu->addAction(resetsession);
	modmenu->addAction(terminatesession);

	sessionmenu->addAction(reportabuse);
	sessionmenu->addSeparator();
	sessionmenu->addAction(gainop);
	sessionmenu->addAction(serverlog);
	sessionmenu->addAction(sessionSettings);
	sessionmenu->addAction(locksession);

	//
	// Tools menu and toolbar
	//
	QAction *freehandtool = makeAction("toolbrush", "draw-brush", tr("Freehand"), tr("Freehand brush tool"), QKeySequence("B"), true);
	QAction *erasertool = makeAction("tooleraser", "draw-eraser", tr("Eraser"), tr("Freehand eraser brush"), QKeySequence("E"), true);
	QAction *linetool = makeAction("toolline", "draw-line", tr("&Line"), tr("Draw straight lines"), QKeySequence("U"), true);
	QAction *recttool = makeAction("toolrect", "draw-rectangle", tr("&Rectangle"), tr("Draw unfilled squares and rectangles"), QKeySequence("R"), true);
	QAction *ellipsetool = makeAction("toolellipse", "draw-ellipse", tr("&Ellipse"), tr("Draw unfilled circles and ellipses"), QKeySequence("O"), true);
	QAction *beziertool = makeAction("toolbezier", "draw-bezier-curves", tr("Bezier Curve"), tr("Draw bezier curves"), QKeySequence("Ctrl+B"), true);
	QAction *filltool = makeAction("toolfill", "fill-color", tr("&Flood Fill"), tr("Fill areas"), QKeySequence("F"), true);
	QAction *annotationtool = makeAction("tooltext", "draw-text", tr("&Annotation"), tr("Add text to the picture"), QKeySequence("A"), true);

	QAction *pickertool = makeAction("toolpicker", "color-picker", tr("&Color Picker"), tr("Pick colors from the image"), QKeySequence("I"), true);
	QAction *lasertool = makeAction("toollaser", "cursor-arrow", tr("&Laser Pointer"), tr("Point out things on the canvas"), QKeySequence("L"), true);
	QAction *selectiontool = makeAction("toolselectrect", "select-rectangular", tr("&Select (Rectangular)"), tr("Select area for copying"), QKeySequence("S"), true);
	QAction *lassotool = makeAction("toolselectpolygon", "edit-select-lasso", tr("&Select (Free-Form)"), tr("Select a free-form area for copying"), QKeySequence("D"), true);
	QAction *markertool = makeAction("toolmarker", "flag-red", tr("&Mark"), tr("Leave a marker to find this spot on the recording"), QKeySequence("Ctrl+M"));

	connect(markertool, SIGNAL(triggered()), this, SLOT(markSpotForRecording()));

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

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(m_drawingtools->actions());
	toolsmenu->addAction(markertool);

	QMenu *toolshortcuts = toolsmenu->addMenu(tr("&Shortcuts"));

	QAction *currentEraseMode = makeAction("currenterasemode", 0, tr("Toggle eraser mode"), QString(), QKeySequence("Ctrl+E"));
	QAction *swapcolors = makeAction("swapcolors", 0, tr("Swap Last Colors"), QString(), QKeySequence("X"));
	QAction *smallerbrush = makeAction("ensmallenbrush", 0, tr("&Decrease Brush Size"), QString(), Qt::Key_BracketLeft);
	QAction *biggerbrush = makeAction("embiggenbrush", 0, tr("&Increase Brush Size"), QString(), Qt::Key_BracketRight);

	QAction *layerUpAct = makeAction("layer-up", nullptr, tr("Select Layer Above"), QString(), QKeySequence("Shift+X"));
	QAction *layerDownAct = makeAction("layer-down", nullptr, tr("Select Layer Below"), QString(), QKeySequence("Shift+Z"));

	smallerbrush->setAutoRepeat(true);
	biggerbrush->setAutoRepeat(true);

	connect(currentEraseMode, &QAction::triggered, _dock_toolsettings, &docks::ToolSettings::toggleEraserMode);
	connect(swapcolors, &QAction::triggered, _dock_colors, &docks::ColorBox::swapLastUsedColors);
	connect(smallerbrush, &QAction::triggered, this, [this]() { _dock_toolsettings->quickAdjustCurrent1(-1); });
	connect(biggerbrush, &QAction::triggered, this, [this]() { _dock_toolsettings->quickAdjustCurrent1(1); });
	connect(layerUpAct, &QAction::triggered, _dock_layers, &docks::LayerList::selectAbove);
	connect(layerDownAct, &QAction::triggered, _dock_layers, &docks::LayerList::selectBelow);

	toolshortcuts->addAction(currentEraseMode);
	toolshortcuts->addAction(swapcolors);
	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);
	toolshortcuts->addSeparator();
	toolshortcuts->addAction(layerUpAct);
	toolshortcuts->addAction(layerDownAct);

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
	QAction *homepage = makeAction("dphomepage", 0, tr("&Homepage"), WEBSITE);
	QAction *tablettester = makeAction("tablettester", 0, tr("Tablet Tester"));
	QAction *showlogfile = makeAction("showlogfile", 0, tr("Log File"));
	QAction *about = makeAction("dpabout", 0, tr("&About Drawpile")); about->setMenuRole(QAction::AboutRole);
	QAction *aboutqt = makeAction("aboutqt", 0, tr("About &Qt")); aboutqt->setMenuRole(QAction::AboutQtRole);

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);
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
		_dock_toolsettings->setToolSlot(a->property("toolslotidx").toInt());
		_toolChangeTime.start();
	});

	// Add temporary tool change shortcut detector
	for(QAction *act : m_drawingtools->actions())
		act->installEventFilter(_tempToolSwitchShortcut);

	for(QAction *act : m_brushSlots->actions())
		act->installEventFilter(_tempToolSwitchShortcut);

	// Other shortcuts
	QAction *finishStrokeShortcut = makeAction("finishstroke", 0, tr("Finish action"), QString(), QKeySequence(Qt::Key_Return));
	connect(finishStrokeShortcut, &QAction::triggered,
			m_doc->toolCtrl(), &tools::ToolController::finishMultipartDrawing);

	QAction *escapeShortcut = makeAction("cancelaction", 0, tr("Cancel action"), QString(), QKeySequence(Qt::Key_Escape));
	connect(escapeShortcut, &QAction::triggered,
			m_doc->toolCtrl(), &tools::ToolController::cancelMultipartDrawing);
}

void MainWindow::createDocks()
{
	// Create tool settings
	_dock_toolsettings = new docks::ToolSettings(m_doc->toolCtrl(), this);
	_dock_toolsettings->setObjectName("ToolSettings");
	_dock_toolsettings->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_toolsettings);

	// Create brush palette
	m_dockBrushPalette = new docks::BrushPalette(this);
	m_dockBrushPalette->setObjectName("BrushPalette");
	m_dockBrushPalette->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, m_dockBrushPalette);

	m_dockBrushPalette->connectBrushSettings(_dock_toolsettings->getToolSettingsPage(tools::Tool::FREEHAND));

	// Create color box
	_dock_colors = new docks::ColorBox(tr("Color"), this);
	_dock_colors->setObjectName("colordock");

	addDockWidget(Qt::RightDockWidgetArea, _dock_colors);

	// Create layer list
	_dock_layers = new docks::LayerList(this);
	_dock_layers->setObjectName("LayerList");
	_dock_layers->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_layers);

	// Create navigator
	_dock_navigator = new docks::Navigator(this);
	_dock_navigator->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_navigator);
	_dock_navigator->hide(); // hidden by default

	// Create input settings
	_dock_input = new docks::InputSettings(this);
	_dock_input->setObjectName("InputSettings");
	_dock_input->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_input);

	// Tabify docks
	tabifyDockWidget(_dock_layers, _dock_input);
}
