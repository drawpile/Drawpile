/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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

#ifndef NDEBUG
#include <QTimer>
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
#include "loader.h"

#include "core/layerstack.h"
#include "scene/canvasview.h"
#include "scene/canvasscene.h"
#include "scene/selectionitem.h"
#include "statetracker.h"
#include "tools/toolsettings.h" // for setting annotation editor widgets Client pointer

#include "utils/recentfiles.h"
#include "utils/whatismyip.h"
#include "utils/icon.h"
#include "utils/networkaccess.h"
#include "utils/shortcutdetector.h"
#include "utils/customshortcutmodel.h"
#include "utils/settings.h"

#include "widgets/viewstatus.h"
#include "widgets/netstatus.h"
#include "widgets/chatwidget.h"

#include "docks/toolsettingsdock.h"
#include "docks/navigator.h"
#include "docks/colorbox.h"
#include "docks/userlistdock.h"
#include "docks/layerlistdock.h"
#include "docks/inputsettingsdock.h"

#include "net/client.h"
#include "net/login.h"
#include "net/serverthread.h"
#include "net/layerlist.h"

#include "../shared/record/writer.h"
#include "../shared/record/reader.h"
#include "../shared/util/filename.h"

#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/resizedialog.h"
#include "dialogs/playbackdialog.h"
#include "dialogs/flipbook.h"

#include "export/animation.h"

namespace {

QString getLastPath() {
	QSettings cfg;
	return cfg.value("window/lastpath").toString();
}

void setLastPath(const QString &lastpath) {
	QSettings cfg;
	cfg.setValue("window/lastpath", lastpath);
}

//! Get a whitelisted set of writable image formats
QList<QPair<QString,QByteArray>> writableImageFormats()
{
	QList<QPair<QString,QByteArray>> formats;

	// We support ORA ourselves
	formats.append(QPair<QString,QByteArray>("OpenRaster", "ora"));

	// Get list of available formats
	for(const QByteArray &fmt : QImageWriter::supportedImageFormats())
	{
		// only offer a reasonable subset
		if(fmt == "png" || fmt=="jpeg" || fmt=="bmp" || fmt=="gif" || fmt=="tiff")
			formats.append(QPair<QString,QByteArray>(QString(fmt).toUpper(), fmt));
		else if(fmt=="jp2")
			formats.append(QPair<QString,QByteArray>("JPEG2000", fmt));
	}
	return formats;
}

}

MainWindow::MainWindow(bool restoreWindowPosition)
	: QMainWindow(), _dialog_playback(0), _canvas(0), _recorder(0), _autoRecordOnConnect(false), _lastToolBeforePaste(-1)
{
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

#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	setUnifiedTitleAndToolBarOnMac(true);
#endif

	_tempToolSwitchShortcut = new ShortcutDetector(this);

	// Work area is split between the canvas view and the chatbox
	_splitter = new QSplitter(Qt::Vertical, centralwidget);

	mainwinlayout->addWidget(_splitter);

	// Create custom status bar
	_viewStatusBar = new QStatusBar;
	_viewStatusBar->setSizeGripEnabled(false);
	mainwinlayout->addWidget(_viewStatusBar);

	// Create status indicator widgets
	_viewstatus = new widgets::ViewStatus(this);

	_netstatus = new widgets::NetStatus(this);
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
	_viewStatusBar->addWidget(_statusChatButton);

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
		_viewStatusBar->addPermanentWidget(tilemem);
	}
#endif

	_viewStatusBar->addPermanentWidget(_viewstatus);
	_viewStatusBar->addPermanentWidget(_netstatus);
	_viewStatusBar->addPermanentWidget(_recorderstatus);
	_viewStatusBar->addPermanentWidget(_lockstatus);

	// Create canvas view
	_view = new widgets::CanvasView(this);
	_view->setToolSettings(_dock_toolsettings);
	
	_dock_input->connectCanvasView(_view);
	connect(_dock_layers, SIGNAL(layerSelected(int)), _view, SLOT(selectLayer(int)));
	connect(_dock_layers, SIGNAL(layerSelected(int)), this, SLOT(updateLockWidget()));
	connect(_dock_layers, SIGNAL(layerViewModeSelected(int)), _view, SLOT(setLayerViewMode(int)));

	_splitter->addWidget(_view);
	_splitter->setCollapsible(0, false);

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

	connect(_dock_toolsettings, SIGNAL(toolChanged(tools::Type)), this, SLOT(toolChanged(tools::Type)));
	
	// Create the chatbox
	_chatbox = new widgets::ChatBox(this);
	_splitter->addWidget(_chatbox);

	// Make sure the canvas gets the majority share of the splitter the first time
	_splitter->setStretchFactor(0, 1);
	_splitter->setStretchFactor(1, 0);

	// Create canvas scene
	_canvas = new drawingboard::CanvasScene(this);
	_canvas->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	_view->setCanvas(_canvas);
	_dock_navigator->setScene(_canvas);

	connect(_canvas, SIGNAL(colorPicked(QColor, bool)), _dock_toolsettings->getColorPickerSettings(), SLOT(addColor(QColor)));
	connect(_canvas, &drawingboard::CanvasScene::myAnnotationCreated, _dock_toolsettings->getAnnotationSettings(), &tools::AnnotationSettings::setSelection);
	connect(_canvas, &drawingboard::CanvasScene::myAnnotationCreated, _dock_toolsettings->getAnnotationSettings(), &tools::AnnotationSettings::setFocus);
	connect(_canvas, SIGNAL(layerAutoselectRequest(int)), _dock_layers, SLOT(selectLayer(int)));
	connect(_canvas, SIGNAL(annotationDeleted(int)), _dock_toolsettings->getAnnotationSettings(), SLOT(unselect(int)));
	connect(_canvas, &drawingboard::CanvasScene::canvasModified, [this]() { setWindowModified(true); });
	connect(_canvas, &drawingboard::CanvasScene::selectionRemoved, this, &MainWindow::selectionRemoved);
	connect(_canvas, &drawingboard::CanvasScene::colorPicked, [this](const QColor &c, bool bg) {
		if(bg)
			_dock_toolsettings->setBackgroundColor(c);
		else
			_dock_toolsettings->setForegroundColor(c);
	});

	// Color docks

	connect(_dock_toolsettings, SIGNAL(foregroundColorChanged(QColor)), _dock_colors, SLOT(setColor(QColor)));
	connect(_dock_colors, SIGNAL(colorChanged(QColor)), _dock_toolsettings, SLOT(setForegroundColor(QColor)));

	// Navigator <-> View
	connect(_dock_navigator, SIGNAL(focusMoved(const QPoint&)),
			_view, SLOT(scrollTo(const QPoint&)));
	connect(_view, SIGNAL(viewRectChange(const QPolygonF&)),
			_dock_navigator, SLOT(setViewFocus(const QPolygonF&)));
	connect(_dock_navigator, SIGNAL(wheelZoom(int)), _view, SLOT(zoomSteps(int)));

	// Create the network client
	_client = new net::Client(this);
	_view->setClient(_client);
	_dock_layers->setClient(_client);
	_dock_toolsettings->getAnnotationSettings()->setClient(_client);
	_dock_toolsettings->getAnnotationSettings()->setLayerSelector(_dock_layers);
	_dock_toolsettings->getRectSelectionSettings()->setScene(_canvas);
	_dock_toolsettings->getRectSelectionSettings()->setView(_view);
	_dock_toolsettings->getPolySelectionSettings()->setScene(_canvas);
	_dock_toolsettings->getPolySelectionSettings()->setView(_view);
	_dock_users->setClient(_client);

	_client->layerlist()->setLayerGetter([this](int id)->paintcore::Layer* {
		if(_canvas->layers())
			return _canvas->layers()->getLayer(id);
		return nullptr;
	});

	// Client command receive signals
	connect(_client, SIGNAL(drawingCommandReceived(protocol::MessagePtr)), _canvas, SLOT(handleDrawingCommand(protocol::MessagePtr)));
	connect(_client, SIGNAL(drawingCommandLocal(protocol::MessagePtr)), _canvas, SLOT(handleLocalCommand(protocol::MessagePtr)));
	connect(_client, SIGNAL(userPointerMoved(int,QPointF,int)), _canvas, SLOT(moveUserMarker(int,QPointF,int)));
	connect(_client, SIGNAL(needSnapshot(bool)), _canvas, SLOT(sendSnapshot(bool)));
	connect(_canvas, SIGNAL(newSnapshot(QList<protocol::MessagePtr>)), _client, SLOT(sendSnapshot(QList<protocol::MessagePtr>)));

	connect(_client, SIGNAL(sentColorChange(QColor)), _dock_colors, SLOT(addLastUsedColor(QColor)));

	// Meta commands
	connect(_client, SIGNAL(chatMessageReceived(QString,QString,bool,bool,bool)),
			_chatbox, SLOT(receiveMessage(QString,QString,bool,bool,bool)));
	connect(_client, &net::Client::chatMessageReceived, [this]() {
		// Show a "new message" indicator when the chatbox is collapsed
		if(_splitter->sizes().at(1)==0)
			_statusChatButton->show();
	});

	connect(_client, SIGNAL(markerMessageReceived(QString,QString)),
			_chatbox, SLOT(receiveMarker(QString,QString)));
	connect(_chatbox, SIGNAL(message(QString,bool,bool)), _client, SLOT(sendChat(QString,bool,bool)));
	connect(_chatbox, SIGNAL(opCommand(QString)), _client, SLOT(sendOpCommand(QString)));

	connect(_client, SIGNAL(sessionTitleChange(QString)), this, SLOT(setSessionTitle(QString)));
	connect(_client, SIGNAL(opPrivilegeChange(bool)), this, SLOT(setOperatorMode(bool)));
	connect(_client, SIGNAL(sessionConfChange(bool,bool,bool, bool)), this, SLOT(sessionConfChanged(bool,bool,bool, bool)));
	connect(_client, SIGNAL(lockBitsChanged()), this, SLOT(updateLockWidget()));
	connect(_client, SIGNAL(layerVisibilityChange(int,bool)), this, SLOT(updateLockWidget()));

	// Network status changes
	connect(_client, SIGNAL(serverConnected(QString, int)), this, SLOT(connecting()));
	connect(_client, SIGNAL(serverLoggedin(bool)), this, SLOT(loggedin(bool)));
	connect(_client, SIGNAL(serverDisconnected(QString, QString, bool)), this, SLOT(serverDisconnected(QString, QString, bool)));

	connect(_client, SIGNAL(serverConnected(QString, int)), _netstatus, SLOT(connectingToHost(QString, int)));
	connect(_client, SIGNAL(serverDisconnecting()), _netstatus, SLOT(hostDisconnecting()));
	connect(_client, SIGNAL(serverDisconnected(QString, QString, bool)), _netstatus, SLOT(hostDisconnected()));
	connect(_client, SIGNAL(expectingBytes(int)), _netstatus, SLOT(expectBytes(int)));
	connect(_client, SIGNAL(sendingBytes(int)), _netstatus, SLOT(sendingBytes(int)));
	connect(_client, SIGNAL(bytesReceived(int)), _netstatus, SLOT(bytesReceived(int)));
	connect(_client, SIGNAL(bytesSent(int)), _netstatus, SLOT(bytesSent(int)));
	connect(_client, SIGNAL(lagMeasured(qint64)), _netstatus, SLOT(lagMeasured(qint64)));

	connect(_client, SIGNAL(userJoined(int, QString)), _netstatus, SLOT(join(int, QString)));
	connect(_client, SIGNAL(userLeft(QString)), _netstatus, SLOT(leave(QString)));
	connect(_client, SIGNAL(youWereKicked(QString)), _netstatus, SLOT(kicked(QString)));

	connect(_client, SIGNAL(userJoined(int, QString)), _chatbox, SLOT(userJoined(int, QString)));
	connect(_client, SIGNAL(userLeft(QString)), _chatbox, SLOT(userParted(QString)));
	connect(_client, SIGNAL(youWereKicked(QString)), _chatbox, SLOT(kicked(QString)));

	connect(_client, SIGNAL(userJoined(int,QString)), _canvas, SLOT(setUserMarkerName(int,QString)));

	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateShortcuts()));
	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateTabletSupportMode()));
	connect(qApp, SIGNAL(settingsChanged()), _view, SLOT(updateLayerViewParams()));

	updateTabletSupportMode();

	// Create actions and menus
	setupActions();
	setDrawingToolsEnabled(false);

	// Restore settings
	readSettings(restoreWindowPosition);
	
	// Set status indicators
	updateLockWidget();
	setRecorderStatus(false);

	// Handle eraser event
	connect(qApp, SIGNAL(eraserNear(bool)), _dock_toolsettings, SLOT(eraserNear(bool)));

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
	delete _dialog_playback;

	// Make sure all child dialogs are closed
	foreach(QObject *obj, children()) {
		QDialog *child = qobject_cast<QDialog*>(obj);
		delete child;
	}

	// Cleanly shut down recorder if still active
	if(_recorder)
		_recorder->close();
}

/**
 * @brief Initialize session state
 *
 * If the document in this window cannot be replaced, a new mainwindow is created.
 *
 * @return the MainWindow instance in which the document was loaded or 0 in case of error
 */
MainWindow *MainWindow::loadDocument(SessionLoader &loader)
{
	if(!canReplace()) {
		writeSettings();
		MainWindow *win = new MainWindow(false);
		Q_ASSERT(win->canReplace());
		if(!win->loadDocument(loader)) {
			delete win;
			win = nullptr;
		}
		return win;
	}

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	QList<protocol::MessagePtr> init = loader.loadInitCommands();

	if(init.isEmpty()) {
		QApplication::restoreOverrideCursor();
		showErrorMessage(loader.errorMessage());
		return nullptr;
	}

	_canvas->initCanvas(_client);
	_dock_layers->init();
	_client->init();
	
	// Set local history size limit. This must be at least as big as the initializer,
	// otherwise a new snapshot will always have to be generated when hosting a session.
	uint minsizelimit = 0;
	foreach(protocol::MessagePtr msg, init)
		minsizelimit += msg->length();
	minsizelimit *= 2;

	_canvas->statetracker()->setMaxHistorySize(qMax(1024*1024*10u, minsizelimit));
	_client->sendLocalInit(init);

	QApplication::restoreOverrideCursor();

	_current_filename = loader.filename();
	setWindowModified(false);
	updateTitle();
	_currentdoctools->setEnabled(true);
	_docadmintools->setEnabled(true);
	setDrawingToolsEnabled(true);

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

	_canvas->initCanvas(_client);
	_dock_layers->init();
	_client->init();

	_canvas->statetracker()->setMaxHistorySize(1024*1024*10u);
	_canvas->statetracker()->setShowAllUserMarkers(true);

	_current_filename = QString();
	setWindowModified(false);
	updateTitle();
	_currentdoctools->setEnabled(true);
	_docadmintools->setEnabled(true);
	setDrawingToolsEnabled(true);

	QFileInfo fileinfo(reader->filename());

	_dialog_playback = new dialogs::PlaybackDialog(_canvas, reader, this);
	_dialog_playback->setWindowTitle(fileinfo.baseName() + " - " + _dialog_playback->windowTitle());
	_dialog_playback->setAttribute(Qt::WA_DeleteOnClose);

	connect(_dialog_playback, &dialogs::PlaybackDialog::commandRead, _client, &net::Client::playbackCommand);
	connect(_dialog_playback, SIGNAL(playbackToggled(bool)), this, SLOT(setRecorderStatus(bool))); // note: the argument goes unused in this case
	connect(_dialog_playback, &dialogs::PlaybackDialog::destroyed, [this]() {
		_dialog_playback = 0;
		getAction("recordsession")->setEnabled(true);
		setRecorderStatus(false);
		_canvas->statetracker()->setShowAllUserMarkers(false);
		_client->endPlayback();
		_canvas->statetracker()->endPlayback();
	});

	_dialog_playback->show();
	_dialog_playback->centerOnParent();

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
	return !(isWindowModified() || _client->isConnected() || _recorder || _dialog_playback);
}

/**
 * The file is added to the list of recent files and the menus on all open
 * mainwindows are updated.
 * @param file filename to add
 */
void MainWindow::addRecentFile(const QString& file)
{
	RecentFiles::addFile(file);
	foreach(QWidget *widget, QApplication::topLevelWidgets()) {
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
	if(_current_filename.isEmpty()) {
		name = tr("Untitled");
	} else {
		const QFileInfo info(_current_filename);
		name = info.baseName();
	}

	if(!_canvas || _canvas->title().isEmpty())
		setWindowTitle(QStringLiteral("%1[*]").arg(name));
	else
		setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, _canvas->title()));

#ifdef Q_OS_MAC
	MacMenu::instance()->updateWindow(this);
#endif
}

void MainWindow::setDrawingToolsEnabled(bool enable)
{
	_drawingtools->setEnabled(enable && _canvas->hasImage());
}

/**
 * Load customized shortcuts
 */
void MainWindow::loadShortcuts()
{
	QSettings cfg;
	cfg.beginGroup("settings/shortcuts");

	QList<QAction*> actions = findChildren<QAction*>();
	foreach(QAction *a, actions) {
		if(!a->objectName().isEmpty() && cfg.contains(a->objectName())) {
			a->setShortcut(cfg.value(a->objectName()).value<QKeySequence>());
		}
	}
}

/**
 * Reload shortcuts after they have been changed, for all open main windows
 */
void MainWindow::updateShortcuts()
{
	foreach(QWidget *widget, QApplication::topLevelWidgets()) {
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

	bool enable = cfg.value("tabletevents", true).toBool();
	bool bugs = cfg.value("tabletbugs", false).toBool();

	widgets::CanvasView::TabletMode mode;
	if(!enable)
		mode = widgets::CanvasView::DISABLE_TABLET;
	else if(bugs)
		mode = widgets::CanvasView::HYBRID_TABLET;
	else
		mode = widgets::CanvasView::ENABLE_TABLET;

	_view->setTabletMode(mode);
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

	// Restore view settings
	bool pixelgrid = cfg.value("showgrid", true).toBool();
	getAction("showgrid")->setChecked(pixelgrid);
	_view->setPixelGrid(pixelgrid);

	cfg.endGroup();

	// Restore tool settings
	_dock_toolsettings->readSettings();

	// Restore cursor settings
	bool brushcrosshair = cfg.value("tools/crosshair", false).toBool();
	getAction("brushcrosshair")->setChecked(brushcrosshair);
	_view->setCrosshair(brushcrosshair);

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

	cfg.setValue("showgrid", getAction("showgrid")->isChecked());
	cfg.endGroup();
	_dock_toolsettings->saveSettings();
	cfg.setValue("tools/crosshair", getAction("brushcrosshair")->isChecked());
}

/**
 * Confirm exit. A confirmation dialog is popped up if there are unsaved
 * changes or network connection is open.
 * @param event event info
 */
void MainWindow::closeEvent(QCloseEvent *event)
{
	if(canReplace() == false) {

		// First confirm disconnection
		if(_client->isLoggedIn()) {
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
				_client->disconnectFromServer();
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
			if(box.clickedButton() == savebtn)
				cancel = !save();

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
		_viewStatusBar->showMessage(static_cast<QStatusTipEvent*>(event)->tip());
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
					// Return from temporary tool change
					for(const QAction *act : _drawingtools->actions()) {
						const QKeySequence &seq = act->shortcut();
						if(seq.count()==1 && e->key() == seq[0]) {
							_dock_toolsettings->setPreviousTool();
							break;
						}
					}

					// Return from temporary tool slot change
					for(const QAction *act : _toolslotactions->actions()) {
						const QKeySequence &seq = act->shortcut();
						if(seq.count()==1 && e->key() == seq[0]) {
							_dock_toolsettings->setPreviousToolSlot();
							break;
						}
					}
				}
				_tempToolSwitchShortcut->reset();
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
   BlankCanvasLoader bcl(size, background);
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
			ImageCanvasLoader icl(file);
			if(loadDocument(icl))
				addRecentFile(file);
		}
	} else {
		networkaccess::getFile(url, QString(), _netstatus, [this](const QFile &file, const QString &error) {
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
	QString dprecs = "*.dptxt *.dprec *.dprecz *.dprec.gz ";
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	const QString filter =
			tr("All Supported Files (%1)").arg(dpimages + dprecs + formats) + ";;" +
			tr("Images (%1)").arg(dpimages + formats) + ";;" +
			tr("Recordings (%1)").arg(dprecs) + ";;" +
			QApplication::tr("All Files (*)");

	// Get the file name to open
#if (QT_VERSION >= QT_VERSION_CHECK(5, 2, 0))
	const QUrl file = QFileDialog::getOpenFileUrl(this,
			tr("Open Image"), getLastPath(), filter);
#else
	const QUrl file = QUrl::fromLocalFile(QFileDialog::getOpenFileName(this,
			tr("Open Image"), getLastPath(), filter));
#endif

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
bool MainWindow::save()
{
	if(_current_filename.isEmpty()) {
		return saveas();
	} else {
		QByteArray suffix = QFileInfo(_current_filename).suffix().toLower().toUtf8();

		// Check if suffix is one of the supported formats
		// If not, we need to ask for a new file name
		bool isSupported = false;
		for(const QPair<QString,QByteArray> &fmt : writableImageFormats()) {
			if(fmt.second == suffix) {
				isSupported = true;
				break;
			}
		}
		if(!isSupported)
			return saveas();

		// Check if features that need OpenRaster format are used
		if(suffix != "ora" && _canvas->needSaveOra()) {
			if(confirmFlatten(_current_filename)==false)
				return false;
		}

		// Overwrite current file
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		bool saved = _canvas->save(_current_filename);
		QApplication::restoreOverrideCursor();
		if(!saved) {
			showErrorMessage(tr("Couldn't save image"));
			return false;
		} else {
			setWindowModified(false);
			addRecentFile(_current_filename);
			return true;
		}
	}
}

/**
 * A standard file dialog is used to get the name of the file to save.
 * If no suffix is the suffix from the current filter is used.
 */
bool MainWindow::saveas()
{
	QString selfilter;
	QStringList filter;

	// Get a list of all supported formats
	for(const QPair<QString,QByteArray> &format : writableImageFormats()) {
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
				if(_canvas->needSaveOra())
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
		if(_canvas->needSaveOra() && !file.endsWith(".ora", Qt::CaseInsensitive)) {
			if(confirmFlatten(file)==false)
				return false;
		}

		// Save the image
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		bool saved = _canvas->save(file);
		QApplication::restoreOverrideCursor();
		if(!saved) {
			showErrorMessage(tr("Couldn't save image"));
			return false;
		} else {
			_current_filename = file;
			setWindowModified(false);
			updateTitle();
			return true;
		}
	}
	return false;
}

void MainWindow::exportAnimation()
{
	AnimationExporter::exportAnimation(_canvas->layers(), this);
}

void MainWindow::showFlipbook()
{
	dialogs::Flipbook *fp = new dialogs::Flipbook(this);
	fp->setAttribute(Qt::WA_DeleteOnClose);
	fp->setLayers(_canvas->layers());
	fp->show();
}

void MainWindow::setRecorderStatus(bool on)
{
	if(_dialog_playback) {
		if(_dialog_playback->isPlaying()) {
			_recorderstatus->setPixmap(icon::fromTheme("media-playback-start").pixmap(16, 16));
			_recorderstatus->setToolTip("Playing back recording");
		} else {
			_recorderstatus->setPixmap(icon::fromTheme("media-playback-pause").pixmap(16, 16));
			_recorderstatus->setToolTip("Playback paused");
		}
		_recorderstatus->show();
	} else {
		if(on) {
			QIcon icon = icon::fromTheme("media-record");
			_recorderstatus->setPixmap(icon.pixmap(16, 16));
			_recorderstatus->setToolTip("Recording session");
			_recorderstatus->show();
		} else {
			_recorderstatus->hide();
		}

		getAction("toolmarker")->setEnabled(on);
	}
}

void MainWindow::toggleRecording()
{
	QAction *recordAction = getAction("recordsession");

	if(_recorder) {
		_recorder->close();
		delete _recorder;
		_recorder = 0;

		recordAction->setText("Record...");
		recordAction->setIcon(icon::fromTheme("media-record"));
		setRecorderStatus(false);
		return;
	}

	QString filter =
			tr("Recordings (%1)").arg("*.dprec") + ";;" +
			tr("Compressed Recordings (%1)").arg("*.dprecz") + ";;" +
			QApplication::tr("All Files (*)");
	QString file = QFileDialog::getSaveFileName(this,
			tr("Record Session"), getLastPath(), filter);

	if(!file.isEmpty()) {
		startRecorder(file);
	}
}

void MainWindow::startRecorder(const QString &filename)
{
	Q_ASSERT(!_recorder);
	QAction *recordAction = getAction("recordsession");

	// Set file suffix if missing
	QString file = filename;
	const QFileInfo info(file);
	if(info.suffix().isEmpty())
		file += ".dprec";

	// Start the recorder
	_recorder = new recording::Writer(file, this);

	if(!_recorder->open()) {
		showErrorMessage(_recorder->errorString());
		delete _recorder;
		_recorder = 0;
	} else {
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		_recorder->writeHeader();

		QList<protocol::MessagePtr> snapshot = _canvas->statetracker()->generateSnapshot(false);
		foreach(const protocol::MessagePtr ptr, snapshot) {
			_recorder->recordMessage(ptr);
		}

		QSettings cfg;
		cfg.beginGroup("settings/recording");
		if(cfg.value("recordpause", true).toBool())
			_recorder->setMinimumInterval(1000 * cfg.value("minimumpause", 0.5).toFloat());

		connect(_client, SIGNAL(messageReceived(protocol::MessagePtr)), _recorder, SLOT(recordMessage(protocol::MessagePtr)));

		recordAction->setText(tr("Stop Recording"));
		recordAction->setIcon(icon::fromTheme("media-playback-stop"));

		QApplication::restoreOverrideCursor();
		setRecorderStatus(true);
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
	auto dlg = new dialogs::HostDialog(this);

	connect(dlg, &dialogs::HostDialog::finished, [this, dlg](int i) {
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
		address.setHost(WhatIsMyIp::localAddress());
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
	login->setSessionId(dlg->getSessionId());
	login->setPassword(dlg->getPassword());
	login->setTitle(dlg->getTitle());
	login->setMaxUsers(dlg->getUserLimit());
	login->setAllowDrawing(dlg->getAllowDrawing());
	login->setLayerControlLock(dlg->getLayerControlLock());
	login->setPersistentSessions(dlg->getPersistentMode());
	login->setPreserveChat(dlg->getPreserveChat());
	login->setAnnounceUrl(dlg->getAnnouncementUrl());
	_client->connectToServer(login);
}

/**
 * Show the join dialog
 */
void MainWindow::join()
{
	auto dlg = new dialogs::JoinDialog(this);
	connect(dlg, &dialogs::JoinDialog::finished, [this, dlg](int i) {
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
		_canvas->title().isEmpty()?tr("Untitled"):_canvas->title(),
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
	connect(leavebox, &QMessageBox::finished, [this](int result) {
		if(result == 0)
			_client->disconnectFromServer();
	});
	
	if(_client->uploadQueueBytes() > 0) {
		leavebox->setIcon(QMessageBox::Warning);
		leavebox->setInformativeText(tr("There is still unsent data! Please wait until transmission completes!"));
	}

	leavebox->show();
}

void MainWindow::changeSessionTitle()
{
	bool ok;
	QString newtitle = QInputDialog::getText(
				this,
				tr("Session Title"),
				tr("Change session title"),
				QLineEdit::Normal,
				_canvas->title(),
				&ok
	);
	if(ok && newtitle != _canvas->title()) {
		_client->sendSetSessionTitle(newtitle);
	}
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

	_autoRecordOnConnect = autoRecord;
	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::JOIN, url, this);
	_client->connectToServer(login);
}

/**
 * Now connecting to server
 */
void MainWindow::connecting()
{
	// Enable connection related actions
	getAction("hostsession")->setEnabled(false);
	getAction("leavesession")->setEnabled(true);

	// Disable UI until login completes
	_view->setEnabled(false);
	setDrawingToolsEnabled(false);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect)
{
	getAction("hostsession")->setEnabled(true);
	getAction("leavesession")->setEnabled(false);
	_admintools->setEnabled(false);
	_docadmintools->setEnabled(true);

	// Re-enable UI
	_view->setEnabled(true);
	setDrawingToolsEnabled(true);

	setSessionTitle(QString());

	// Make sure all drawing is complete
	if(_canvas->hasImage())
		_canvas->statetracker()->endRemoteContexts();

	// Display login error if not yet logged in
	if(!_client->isLoggedIn() && !localDisconnect) {
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

			QUrl url = _client->sessionUrl(true);

			connect(joinbutton, &QAbstractButton::clicked, [this, url]() {
				joinSession(url);
			});

		}

		msgbox->show();
	}
}

/**
 * Server connection established and login successfull
 */
void MainWindow::loggedin(bool join)
{
	// Update netstatus widget
	_netstatus->loggedIn(_client->sessionUrl());
	_netstatus->setSecurityLevel(_client->securityLevel(), _client->hostCertificate());

	// Re-enable UI
	_view->setEnabled(true);

	// Initialize the canvas (in host mode the canvas was prepared already)
	if(join) {
		_canvas->initCanvas(_client);
		_dock_layers->init();
		_currentdoctools->setEnabled(true);
	}

	// Automatically start recording
	if(_autoRecordOnConnect)
		startRecorder(utils::uniqueFilename(utils::settings::recordingFolder(), "session-" + _client->sessionId(), "dprec"));

	setDrawingToolsEnabled(true);
}


void MainWindow::sessionConfChanged(bool locked, bool layerctrllocked, bool closed, bool preservechat)
{
	getAction("locksession")->setChecked(locked);
	getAction("locklayerctrl")->setChecked(layerctrllocked);
	getAction("denyjoins")->setChecked(closed);
	_dock_layers->setControlsLocked(layerctrllocked);
	_chatbox->setPreserveMode(preservechat);
}

void MainWindow::updateLockWidget()
{
	bool locked = _client->isLocked() || _dock_layers->isCurrentLayerLocked();
	if(locked) {
		_lockstatus->setPixmap(icon::fromTheme("object-locked").pixmap(16, 16));
		_lockstatus->setToolTip(tr("Board is locked"));
	} else {
		_lockstatus->setPixmap(QPixmap());
		_lockstatus->setToolTip(QString());
	}
	_view->setLocked(locked);
}

/**
 * Session title changed
 * @param title new title
 */
void MainWindow::setSessionTitle(const QString& title)
{
	_canvas->setTitle(title);
	updateTitle();
}

void MainWindow::setOperatorMode(bool op)
{
	_admintools->setEnabled(op);
	_docadmintools->setEnabled(op);
	_dock_layers->setOperatorMode(op);
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
	_canvas->showAnnotations(show);
	if(!show) {
		if(annotationtool->isChecked())
			getAction("toolbrush")->trigger();
		_dock_toolsettings->disableEraserOverride(tools::ANNOTATION);
	}
}

void MainWindow::setShowLaserTrails(bool show)
{
	QAction *lasertool = getAction("toollaser");
	lasertool->setEnabled(show);
	_canvas->showLaserTrails(show);
	if(!show) {
		if(lasertool->isChecked())
			getAction("toolbrush")->trigger();
		_dock_toolsettings->disableEraserOverride(tools::LASERPOINTER);
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
		_fullscreen_oldstate = saveState();
		_fullscreen_oldgeometry = geometry();

		// Hide everything except floating docks
		menuBar()->hide();
		_viewStatusBar->hide();
		_view->setFrameShape(QFrame::NoFrame);
		foreach(QObject *child, children()) {
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
		showNormal();
		menuBar()->show();
		_viewStatusBar->show();
		_view->setFrameShape(QFrame::StyledPanel);
		setGeometry(_fullscreen_oldgeometry);
		restoreState(_fullscreen_oldstate);
	}
}

void MainWindow::hotBorderMenubar(bool show)
{
	if(windowState().testFlag(Qt::WindowFullScreen)) {
		menuBar()->setVisible(show);
	}
}

/**
 * User selected a tool
 * @param tool action representing the tool
 */
void MainWindow::selectTool(QAction *tool)
{
	// Note. Actions must be in the same order in the enum and the group
	int idx = _drawingtools->actions().indexOf(tool);
	Q_ASSERT(idx>=0);
	if(idx<0)
		return;

	_dock_toolsettings->setTool(tools::Type(idx));
	_toolChangeTime.start();
	_lastToolBeforePaste = -1;
}

/**
 * @brief Handle tool change
 * @param tool
 */
void MainWindow::toolChanged(tools::Type tool)
{
	QAction *toolaction = _drawingtools->actions().at(int(tool));
	toolaction->setChecked(true);

	// When using the annotation tool, highlight all text boxes
	_canvas->showAnnotationBorders(tool==tools::ANNOTATION);

	// Send pointer updates when using the laser pointer (TODO checkbox)
	_view->setPointerTracking(tool==tools::LASERPOINTER && _dock_toolsettings->getLaserPointerSettings()->pointerTracking());

	// Remove selection when not using selection tool
	if(tool != tools::SELECTION)
		_canvas->setSelectionItem(0);

	_view->selectTool(tool);
}

void MainWindow::selectionRemoved()
{
	if(_lastToolBeforePaste>=0) {
		// Selection was just removed and we had just pasted an image
		// so restore the previously used tool
		QAction *toolaction = _drawingtools->actions().at(_lastToolBeforePaste);
		toolaction->trigger();
	}
}

void MainWindow::selectAll()
{
	getAction("toolselectrect")->trigger();
	_canvas->setSelectionItem(QRect(QPoint(), _canvas->imageSize()));
}

void MainWindow::selectNone()
{
	if(_canvas->selectionItem()) {
		_canvas->selectionItem()->pasteToCanvas(_client, _dock_layers->currentLayer());
		_canvas->setSelectionItem(0);
	}
}

void MainWindow::cutLayer()
{
	QImage img = _canvas->selectionToImage(_dock_layers->currentLayer());
	fillArea(Qt::transparent);
	QApplication::clipboard()->setImage(img);
}

void MainWindow::copyLayer()
{
	QImage img = _canvas->selectionToImage(_dock_layers->currentLayer());
	QApplication::clipboard()->setImage(img);
}

void MainWindow::copyVisible()
{
	QImage img = _canvas->selectionToImage(0);
	QApplication::clipboard()->setImage(img);
}

void MainWindow::paste()
{
	QImage img = QApplication::clipboard()->image();
	if(img.isNull())
		return;

	pasteImage(img);
}

void MainWindow::pasteFile()
{
	// Get a list of supported formats
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
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
		networkaccess::getImage(url, _netstatus, [this](const QImage &image, const QString &error) {
			if(image.isNull())
				showErrorMessage(error);
			else
				pasteImage(image);
		});
	}
}

void MainWindow::pasteImage(const QImage &image)
{
	if(_canvas->hasImage()) {
		if(_dock_toolsettings->currentTool() != tools::SELECTION && _dock_toolsettings->currentTool() != tools::POLYGONSELECTION) {
			int currentTool = _dock_toolsettings->currentTool();
			getAction("toolselectrect")->trigger();
			_lastToolBeforePaste = currentTool;
		}

		_canvas->pasteFromImage(image, _view->viewCenterPoint());
	} else {
		// Canvas not yet initialized? Initialize with clipboard content
		QImageCanvasLoader loader(image);
		loadDocument(loader);
	}
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
		networkaccess::getFile(url, "", _netstatus, [this](const QFile &file, const QString &error) {
			if(error.isEmpty())
				dropUrl(QUrl::fromLocalFile(file.fileName()));
			else
				showErrorMessage(error);
		});
	}
}

void MainWindow::removeEmptyAnnotations()
{
	QList<int> ids = _canvas->listEmptyAnnotations();
	if(!ids.isEmpty()) {
		_client->sendUndopoint();
		foreach(int id, ids)
			_client->sendAnnotationDelete(id);
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
		int a = _dock_toolsettings->getAnnotationSettings()->selected();
		if(a>0) {
			_client->sendUndopoint();
			_client->sendAnnotationDelete(a);
			return;
		}
	}

	// No annotation selected: clear seleted area as usual
	fillArea(Qt::transparent);
}

void MainWindow::fillFgArea()
{
	fillArea(_dock_toolsettings->foregroundColor());
}

void MainWindow::fillBgArea()
{
	fillArea(_dock_toolsettings->backgroundColor());
}

void MainWindow::fillArea(const QColor &color)
{
	if(_canvas->selectionItem()) {
		// Selection exists: fill selected area only
		_canvas->selectionItem()->fillCanvas(color, _client, _dock_layers->currentLayer());

	} else {
		// No selection: fill entire layer
		_client->sendUndopoint();
		_client->sendFillRect(_dock_layers->currentLayer(), QRect(QPoint(), _canvas->imageSize()), color);
	}
}

void MainWindow::resizeCanvas()
{
	QSize size = _canvas->imageSize();
	dialogs::ResizeDialog *dlg = new dialogs::ResizeDialog(size, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);

	connect(dlg, &QDialog::accepted, [this, dlg]() {
		dialogs::ResizeVector r = dlg->resizeVector();
		if(!r.isZero()) {
			_client->sendUndopoint();
			_client->sendCanvasResize(r.top, r.right, r.bottom, r.left);
		}
	});
	dlg->show();
}

void MainWindow::markSpotForRecording()
{
	bool ok;
	QString text = QInputDialog::getText(this, tr("Mark"), tr("Marker text"), QLineEdit::Normal, QString(), &ok);
	if(ok)
		_client->sendMarker(text);
}

void MainWindow::about()
{
	QMessageBox::about(0, tr("About Drawpile"),
			QStringLiteral("<p><b>Drawpile %1</b><br>").arg(DRAWPILE_VERSION) +
			tr("A collaborative drawing program.") + QStringLiteral("</p>"

			"<p>Copyright © 2006-2015 Calle Laakkonen</p>"

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

	_admintools = new QActionGroup(this);
	_admintools->setExclusive(false);

	_docadmintools = new QActionGroup(this);
	_docadmintools->setExclusive(false);
	_docadmintools->setEnabled(false);

	_drawingtools = new QActionGroup(this);
	connect(_drawingtools, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	QMenu *toggletoolbarmenu = new QMenu(this);
	QMenu *toggledockmenu = new QMenu(this);

	// Collect list of docks for dock menu
	foreach(QObject *c, children()) {
		QDockWidget *dw = qobject_cast<QDockWidget*>(c);
		if(dw)
			toggledockmenu->addAction(dw->toggleViewAction());
	}

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

	connect(_recent, &QMenu::triggered, [this](QAction *action) {
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
	QAction *pastefile = makeAction("pastefile", "document-open", tr("Paste &From File..."));
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", 0, tr("Delete Empty Annotations"));
	QAction *resize = makeAction("resizecanvas", 0, tr("Resi&ze Canvas..."));
	QAction *preferences = makeAction(0, 0, tr("Prefere&nces")); preferences->setMenuRole(QAction::PreferencesRole);

	QAction *selectall = makeAction("selectall", 0, tr("Select &All"), QString(), QKeySequence::SelectAll);
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0) || defined(Q_OS_MAC) || defined(Q_OS_WIN)) // Deselect is not defined on Mac and Win
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"), QString(), QKeySequence("Shift+Ctrl+A"));
#else
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"), QString(), QKeySequence::Deselect);
#endif

	QAction *expandup = makeAction("expandup", 0, tr("Expand &Up"), "", QKeySequence(CTRL_KEY "+J"));
	QAction *expanddown = makeAction("expanddown", 0, tr("Expand &Down"), "", QKeySequence(CTRL_KEY "+K"));
	QAction *expandleft = makeAction("expandleft", 0, tr("Expand &Left"), "", QKeySequence(CTRL_KEY "+H"));
	QAction *expandright = makeAction("expandright", 0, tr("Expand &Right"), "", QKeySequence(CTRL_KEY "+L"));

	QAction *cleararea = makeAction("cleararea", 0, tr("Clear"), tr("Delete selection"), QKeySequence("Delete"));
	QAction *fillfgarea = makeAction("fillfgarea", 0, tr("Fill with &FG Color"), tr("Fill selected area with foreground color"), QKeySequence(CTRL_KEY "+,"));
	QAction *fillbgarea = makeAction("fillbgarea", 0, tr("Fill with B&G Color"), tr("Fill selected area with background color"), QKeySequence(CTRL_KEY "+."));

	_currentdoctools->addAction(undo);
	_currentdoctools->addAction(redo);
	_currentdoctools->addAction(copy);
	_currentdoctools->addAction(copylayer);
	_currentdoctools->addAction(cutlayer);
	_currentdoctools->addAction(deleteAnnotations);
	_currentdoctools->addAction(cleararea);
	_currentdoctools->addAction(fillfgarea);
	_currentdoctools->addAction(fillbgarea);
	_currentdoctools->addAction(selectall);
	_currentdoctools->addAction(selectnone);

	_docadmintools->addAction(resize);
	_docadmintools->addAction(expandup);
	_docadmintools->addAction(expanddown);
	_docadmintools->addAction(expandleft);
	_docadmintools->addAction(expandright);

	connect(undo, SIGNAL(triggered()), _client, SLOT(sendUndo()));
	connect(redo, SIGNAL(triggered()), _client, SLOT(sendRedo()));
	connect(copy, SIGNAL(triggered()), this, SLOT(copyVisible()));
	connect(copylayer, SIGNAL(triggered()), this, SLOT(copyLayer()));
	connect(cutlayer, SIGNAL(triggered()), this, SLOT(cutLayer()));
	connect(paste, SIGNAL(triggered()), this, SLOT(paste()));
	connect(pastefile, SIGNAL(triggered()), this, SLOT(pasteFile()));
	connect(selectall, SIGNAL(triggered()), this, SLOT(selectAll()));
	connect(selectnone, SIGNAL(triggered()), this, SLOT(selectNone()));
	connect(deleteAnnotations, SIGNAL(triggered()), this, SLOT(removeEmptyAnnotations()));
	connect(cleararea, SIGNAL(triggered()), this, SLOT(clearOrDelete()));
	connect(fillfgarea, SIGNAL(triggered()), this, SLOT(fillFgArea()));
	connect(fillbgarea, SIGNAL(triggered()), this, SLOT(fillBgArea()));
	connect(resize, SIGNAL(triggered()), this, SLOT(resizeCanvas()));
	connect(preferences, SIGNAL(triggered()), this, SLOT(showSettings()));

	// Expanding by multiples of tile size allows efficient resizing
	connect(expandup, &QAction::triggered, [this] { _client->sendUndopoint(); _client->sendCanvasResize(64, 0 ,0, 0);});
	connect(expandright, &QAction::triggered, [this] {_client->sendUndopoint();  _client->sendCanvasResize(0, 64, 0, 0);});
	connect(expanddown, &QAction::triggered, [this] { _client->sendUndopoint(); _client->sendCanvasResize(0,0, 64, 0);});
	connect(expandleft, &QAction::triggered, [this] { _client->sendUndopoint(); _client->sendCanvasResize(0,0, 0, 64);});

	QMenu *editmenu = menuBar()->addMenu(tr("&Edit"));
	editmenu->addAction(undo);
	editmenu->addAction(redo);
	editmenu->addSeparator();
	editmenu->addAction(cutlayer);
	editmenu->addAction(copy);
	editmenu->addAction(copylayer);
	editmenu->addAction(paste);
	editmenu->addAction(pastefile);
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
	editmenu->addAction(fillbgarea);
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
	QAction *rotate90 = makeAction("rotate90", 0, tr("Rotate to 90°"));
	QAction *rotate180 = makeAction("rotate180", 0, tr("Rotate to 180°"));
	QAction *rotate270 = makeAction("rotate270", 0, tr("Rotate to 270°"));

	QAction *viewmirror = makeAction("viewmirror", "object-flip-horizontal", tr("Mirror"), QString(), QKeySequence("V"), true);
	QAction *viewflip = makeAction("viewflip", "object-flip-vertical", tr("Flip"), QString(), QKeySequence("C"), true);

	QAction *showcrosshair = makeAction("brushcrosshair", 0, tr("Show Crosshair C&ursor"), QString(), QKeySequence(), true);
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
		connect(windowHandle(), &QWindow::windowStateChanged, [fullscreen](Qt::WindowState state) {
			// Update the mode tickmark on fulscreen state change.
			// On Qt 5.3.0, this signal doesn't seem to get emitted on OSX when clicking
			// on the toggle button in the titlebar. The state can be queried correctly though.
			fullscreen->setChecked(state & Qt::WindowFullScreen);
		});
	}

	connect(_statusChatButton, &QToolButton::clicked, toggleChat, &QAction::trigger);

	connect(_chatbox, SIGNAL(expanded(bool)), toggleChat, SLOT(setChecked(bool)));
	connect(_chatbox, SIGNAL(expanded(bool)), _statusChatButton, SLOT(hide()));
	connect(toggleChat, &QAction::triggered, [this](bool show) {
		QList<int> sizes;
		if(show) {
			QVariant oldHeight = _chatbox->property("oldheight");
			if(oldHeight.isNull()) {
				const int h = height();
				sizes << h * 2 / 3;
				sizes << h / 3;
			} else {
				const int oh = oldHeight.toInt();
				sizes << height() - oh;
				sizes << oh;
			}
			_chatbox->focusInput();
		} else {
			_chatbox->setProperty("oldheight", _chatbox->height());
			sizes << 1;
			sizes << 0;
		}
		_splitter->setSizes(sizes);
	});

	connect(showFlipbook, SIGNAL(triggered()), this, SLOT(showFlipbook()));

	connect(zoomin, SIGNAL(triggered()), _view, SLOT(zoomin()));
	connect(zoomout, SIGNAL(triggered()), _view, SLOT(zoomout()));
	connect(zoomorig, &QAction::triggered, [this]() { _view->setZoom(100.0); });
	connect(rotateorig, &QAction::triggered, [this]() { _view->setRotation(0); });
	connect(rotate90, &QAction::triggered, [this]() { _view->setRotation(90); });
	connect(rotate180, &QAction::triggered, [this]() { _view->setRotation(180); });
	connect(rotate270, &QAction::triggered, [this]() { _view->setRotation(270); });
	connect(viewflip, SIGNAL(triggered(bool)), _view, SLOT(setViewFlip(bool)));
	connect(viewmirror, SIGNAL(triggered(bool)), _view, SLOT(setViewMirror(bool)));

	connect(fullscreen, SIGNAL(triggered()), this, SLOT(toggleFullscreen()));

	connect(showannotations, SIGNAL(triggered(bool)), this, SLOT(setShowAnnotations(bool)));
	connect(showcrosshair, SIGNAL(triggered(bool)), _view, SLOT(setCrosshair(bool)));
	connect(showusermarkers, SIGNAL(triggered(bool)), _canvas, SLOT(showUserMarkers(bool)));
	connect(showuserlayers, SIGNAL(triggered(bool)), _canvas, SLOT(showUserLayers(bool)));
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
	rotatemenu->addAction(rotate90);
	rotatemenu->addAction(rotate180);
	rotatemenu->addAction(rotate270);

	viewmenu->addAction(viewflip);
	viewmenu->addAction(viewmirror);

	viewmenu->addSeparator();
	viewmenu->addAction(showannotations);
	viewmenu->addAction(showcrosshair);
	viewmenu->addAction(showusermarkers);
	viewmenu->addAction(showuserlayers);
	viewmenu->addAction(showlasers);
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

	QAction *locksession = makeAction("locksession", 0, tr("Lo&ck the Board"), tr("Prevent changes to the drawing board"), QKeySequence("F12"), true);
	QAction *locklayerctrl = makeAction("locklayerctrl", 0, tr("Lock Layer Controls"), tr("Allow only session operators to add and change layers"), QKeySequence(), true);
	QAction *closesession = makeAction("denyjoins", 0, tr("&Deny Joins"), tr("Prevent new users from joining the session"), QKeySequence(), true);

	QAction *changetitle = makeAction("changetitle", 0, tr("Change &Title..."));

	_admintools->addAction(locksession);
	_admintools->addAction(locklayerctrl);
	_admintools->addAction(closesession);
	_admintools->addAction(changetitle);
	_admintools->setEnabled(false);

	connect(host, SIGNAL(triggered()), this, SLOT(host()));
	connect(join, SIGNAL(triggered()), this, SLOT(join()));
	connect(logout, SIGNAL(triggered()), this, SLOT(leave()));
	connect(changetitle, SIGNAL(triggered()), this, SLOT(changeSessionTitle()));
	connect(locksession, SIGNAL(triggered(bool)), _client, SLOT(sendLockSession(bool)));
	connect(locklayerctrl, SIGNAL(triggered(bool)), _client, SLOT(sendLockLayerControls(bool)));
	connect(closesession, SIGNAL(triggered(bool)), _client, SLOT(sendCloseSession(bool)));

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host);
	sessionmenu->addAction(join);
	sessionmenu->addAction(logout);
	sessionmenu->addSeparator();
	sessionmenu->addAction(locksession);
	sessionmenu->addAction(locklayerctrl);
	sessionmenu->addAction(closesession);
	sessionmenu->addAction(changetitle);

	//
	// Tools menu and toolbar
	//
	QAction *pentool = makeAction("toolpen", "draw-freehand", tr("&Pen"), tr("Draw with hard edged strokes"), QKeySequence("P"), true);
	QAction *brushtool = makeAction("toolbrush", "draw-brush", tr("&Brush"), tr("Draw with smooth strokes"), QKeySequence("B"), true);
	QAction *smudgetool = makeAction("toolsmudge", "draw-watercolor", tr("&Watercolor"), tr("A brush that picks up color from the layer"), QKeySequence("W"), true);
	QAction *erasertool = makeAction("tooleraser", "draw-eraser", tr("&Eraser"), tr("Erase layer content"), QKeySequence("E"), true);
	QAction *linetool = makeAction("toolline", "draw-line", tr("&Line"), tr("Draw straight lines"), QKeySequence("U"), true);
	QAction *recttool = makeAction("toolrect", "draw-rectangle", tr("&Rectangle"), tr("Draw unfilled squares and rectangles"), QKeySequence("R"), true);
	QAction *ellipsetool = makeAction("toolellipse", "draw-ellipse", tr("&Ellipse"), tr("Draw unfilled circles and ellipses"), QKeySequence("O"), true);
	QAction *filltool = makeAction("toolfill", "fill-color", tr("&Flood Fill"), tr("Fill areas"), QKeySequence("F"), true);
	QAction *annotationtool = makeAction("tooltext", "draw-text", tr("&Annotation"), tr("Add text to the picture"), QKeySequence("A"), true);

	QAction *pickertool = makeAction("toolpicker", "color-picker", tr("&Color Picker"), tr("Pick colors from the image"), QKeySequence("I"), true);
	QAction *lasertool = makeAction("toollaser", "cursor-arrow", tr("&Laser Pointer"), tr("Point out things on the canvas"), QKeySequence("L"), true);
	QAction *selectiontool = makeAction("toolselectrect", "select-rectangular", tr("&Select (Rectangular)"), tr("Select area for copying"), QKeySequence("S"), true);
	QAction *lassotool = makeAction("toolselectpolygon", "edit-select-lasso", tr("&Select (Free-Form)"), tr("Select a free-form area for copying"), QKeySequence("P"), true);
	QAction *markertool = makeAction("toolmarker", "flag-red", tr("&Mark"), tr("Leave a marker to find this spot on the recording"), QKeySequence("Ctrl+M"));

	connect(markertool, SIGNAL(triggered()), this, SLOT(markSpotForRecording()));

	_drawingtools->addAction(pentool);
	_drawingtools->addAction(brushtool);
	_drawingtools->addAction(smudgetool);
	_drawingtools->addAction(erasertool);
	_drawingtools->addAction(linetool);
	_drawingtools->addAction(recttool);
	_drawingtools->addAction(ellipsetool);
	_drawingtools->addAction(filltool);
	_drawingtools->addAction(annotationtool);
	_drawingtools->addAction(pickertool);
	_drawingtools->addAction(lasertool);
	_drawingtools->addAction(selectiontool);
	_drawingtools->addAction(lassotool);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(_drawingtools->actions());
	toolsmenu->addAction(markertool);
	toolsmenu->addSeparator();

	QMenu *toolshortcuts = toolsmenu->addMenu(tr("&Shortcuts"));

	QAction *swapcolors = makeAction("swapcolors", 0, tr("&Swap Colors"), QString(), QKeySequence(Qt::Key_X));
	QAction *smallerbrush = makeAction("ensmallenbrush", 0, tr("&Decrease Brush Size"), QString(), Qt::Key_BracketLeft);
	QAction *biggerbrush = makeAction("embiggenbrush", 0, tr("&Increase Brush Size"), QString(), Qt::Key_BracketRight);

	smallerbrush->setAutoRepeat(true);
	biggerbrush->setAutoRepeat(true);

	connect(smallerbrush, &QAction::triggered, [this]() { _view->doQuickAdjust1(-1);});
	connect(biggerbrush, &QAction::triggered, [this]() { _view->doQuickAdjust1(1);});

	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);
	toolshortcuts->addAction(swapcolors);

	QToolBar *drawtools = new QToolBar("Drawing tools");
	drawtools->setObjectName("drawtoolsbar");
	toggletoolbarmenu->addAction(drawtools->toggleViewAction());

	// Add a separator before color picker to separate brushes from non-destructive tools
	for(QAction *dt : _drawingtools->actions()) {
		if(dt == pickertool)
			drawtools->addSeparator();
		drawtools->addAction(dt);
	}

	addToolBar(Qt::TopToolBarArea, drawtools);

	connect(swapcolors, SIGNAL(triggered()), _dock_toolsettings, SLOT(swapForegroundBackground()));

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
	QAction *about = makeAction("dpabout", 0, tr("&About Drawpile")); about->setMenuRole(QAction::AboutRole);
	QAction *aboutqt = makeAction("aboutqt", 0, tr("About &Qt")); aboutqt->setMenuRole(QAction::AboutQtRole);

	connect(homepage, &QAction::triggered, &MainWindow::homepage);
	connect(about, &QAction::triggered, &MainWindow::about);
	connect(aboutqt, &QAction::triggered, &QApplication::aboutQt);

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addSeparator();
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);

	//
	// Quick tool change slots
	//
	_toolslotactions = new QActionGroup(this);
	for(int i=0;i<docks::ToolSettings::QUICK_SLOTS;++i) {
		QAction *q = new QAction(QString("Tool slot #%1").arg(i+1), this);
		q->setAutoRepeat(false);
		q->setObjectName(QString("quicktoolslot-%1").arg(i));
		q->setShortcut(QKeySequence(QString::number(i+1)));
		q->setProperty("toolslotidx", i);
		CustomShortcutModel::registerCustomizableAction(q->objectName(), q->text(), q->shortcut());
		_toolslotactions->addAction(q);
		addAction(q);
	}
	connect(_toolslotactions, &QActionGroup::triggered, [this](QAction *a) {
		_dock_toolsettings->setToolSlot(a->property("toolslotidx").toInt());
		_toolChangeTime.start();
	});

	// Quick layer change actions
	QAction *layerUpAct = makeAction("layer-up", nullptr, tr("Select Layer Above"), QString(), QKeySequence(Qt::CTRL | Qt::Key_Up));
	QAction *layerDownAct = makeAction("layer-down", nullptr, tr("Select Layer Below"), QString(), QKeySequence(Qt::CTRL | Qt::Key_Down));
	connect(layerUpAct, &QAction::triggered, _dock_layers, &docks::LayerList::selectAbove);
	connect(layerDownAct, &QAction::triggered, _dock_layers, &docks::LayerList::selectBelow);


	// Add temporary tool change shortcut detector
	for(QAction *act : _drawingtools->actions())
		act->installEventFilter(_tempToolSwitchShortcut);
	for(QAction *act : _toolslotactions->actions())
		act->installEventFilter(_tempToolSwitchShortcut);
}

void MainWindow::createDocks()
{
	// Create tool settings
	_dock_toolsettings = new docks::ToolSettings(this);
	_dock_toolsettings->setObjectName("ToolSettings");
	_dock_toolsettings->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_toolsettings);

	// Create color box
	_dock_colors = new docks::ColorBox(tr("Color"), this);
	_dock_colors->setObjectName("colordock");

	addDockWidget(Qt::RightDockWidgetArea, _dock_colors);

	// Create user list
	_dock_users = new docks::UserList(this);
	_dock_users->setObjectName("userlistdock");
	_dock_users->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_users);

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
	tabifyDockWidget(_dock_users, _dock_layers);
	tabifyDockWidget(_dock_layers, _dock_input);
}
