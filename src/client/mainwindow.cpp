/*
   DrawPile - a collaborative drawing program.

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

#include <QDebug>
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
#include <QImageReader>
#include <QSplitter>
#include <QClipboard>

#include "config.h"
#include "mainwindow.h"
#include "loader.h"

#include "scene/canvasview.h"
#include "scene/canvasscene.h"
#include "scene/selectionitem.h"
#include "scene/strokepreviewer.h"
#include "statetracker.h"
#include "tools/toolsettings.h" // for setting annotation editor widgets Client pointer

#include "utils/recentfiles.h"
#include "utils/whatismyip.h"

#include "widgets/viewstatus.h"
#include "widgets/netstatus.h"
#include "widgets/dualcolorbutton.h"
#include "widgets/chatwidget.h"

#include "docks/toolsettingsdock.h"
#include "docks/palettebox.h"
#include "docks/navigator.h"
#include "docks/colorbox.h"
#include "docks/userlistdock.h"
#include "docks/layerlistdock.h"
#include "docks/inputsettingsdock.h"

#include "net/client.h"
#include "net/login.h"
#include "net/serverthread.h"

#include "../shared/record/writer.h"
#include "../shared/record/reader.h"

#include "dialogs/colordialog.h"
#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/settingsdialog.h"
#include "dialogs/resizedialog.h"
#include "dialogs/playbackdialog.h"

namespace {

QString getLastPath() {
	QSettings cfg;
	return cfg.value("window/lastpath").toString();
}

void setLastPath(const QString &lastpath) {
	QSettings cfg;
	cfg.setValue("window/lastpath", lastpath);
}

}

MainWindow::MainWindow(bool restoreWindowPosition)
	: QMainWindow(), _dialog_playback(0), _canvas(0), _recorder(0)
{
	updateTitle();

	createDocks();

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	// Create status indicator widgets
	auto *viewstatus = new widgets::ViewStatus(this);
	auto *netstatus = new widgets::NetStatus(this);
	_recorderstatus = new QLabel(this);
	_lockstatus = new QLabel(this);

	statusbar->addPermanentWidget(viewstatus);
	statusbar->addPermanentWidget(netstatus);
	statusbar->addPermanentWidget(_recorderstatus);
	statusbar->addPermanentWidget(_lockstatus);

	// Work area is split between the canvas view and the chatbox
	_splitter = new QSplitter(Qt::Vertical, this);
	setCentralWidget(_splitter);

	// Create canvas view
	_view = new widgets::CanvasView(this);
	_view->setToolSettings(_dock_toolsettings);
	
	_dock_input->connectCanvasView(_view);
	connect(_dock_layers, SIGNAL(layerSelected(int)), _view, SLOT(selectLayer(int)));
	connect(_dock_layers, SIGNAL(layerSelected(int)), this, SLOT(updateLockWidget()));

	_splitter->addWidget(_view);
	_splitter->setCollapsible(0, false);

	connect(_dock_toolsettings, SIGNAL(sizeChanged(int)), _view, SLOT(setOutlineRadius(int)));
	connect(_view, SIGNAL(imageDropped(QImage)), this, SLOT(pasteImage(QImage)));
	connect(_view, SIGNAL(urlDropped(QUrl)), this, SLOT(pasteFile(QUrl)));
	connect(_view, SIGNAL(viewTransformed(qreal, qreal)), viewstatus, SLOT(setTransformation(qreal, qreal)));

	connect(this, SIGNAL(toolChanged(tools::Type)), _view, SLOT(selectTool(tools::Type)));
	
	// Create the chatbox
	widgets::ChatBox *chatbox = new widgets::ChatBox(this);
	_splitter->addWidget(chatbox);

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
	connect(_canvas, SIGNAL(myLayerCreated(int)), _dock_layers, SLOT(selectLayer(int)));
	connect(_canvas, SIGNAL(annotationDeleted(int)), _dock_toolsettings->getAnnotationSettings(), SLOT(unselect(int)));
	connect(_canvas, &drawingboard::CanvasScene::canvasModified, [this]() { setWindowModified(true); });

	// Navigator <-> View
	connect(_dock_navigator, SIGNAL(focusMoved(const QPoint&)),
			_view, SLOT(scrollTo(const QPoint&)));
	connect(_dock_navigator, SIGNAL(angleChanged(qreal)),
			_view, SLOT(setRotation(qreal)));
	connect(_view, SIGNAL(viewRectChange(const QPolygonF&)),
			_dock_navigator, SLOT(setViewFocus(const QPolygonF&)));
	connect(_view, SIGNAL(viewTransformed(qreal,qreal)),
			_dock_navigator, SLOT(setViewTransform(qreal,qreal)));
	connect(_dock_navigator, SIGNAL(zoomIn()), _view, SLOT(zoomin()));
	connect(_dock_navigator, SIGNAL(zoomOut()), _view, SLOT(zoomout()));

	// Create the network client
	_client = new net::Client(this);
	_view->setClient(_client);
	_dock_layers->setClient(_client);
	_dock_toolsettings->getAnnotationSettings()->setClient(_client);
	_dock_toolsettings->getAnnotationSettings()->setLayerSelector(_dock_layers);
	_dock_users->setClient(_client);

	// Client command receive signals
	connect(_client, SIGNAL(drawingCommandReceived(protocol::MessagePtr)), _canvas, SLOT(handleDrawingCommand(protocol::MessagePtr)));
	connect(_client, SIGNAL(userPointerMoved(int,QPointF,int)), _canvas, SLOT(moveUserMarker(int,QPointF,int)));
	connect(_client, SIGNAL(needSnapshot(bool)), _canvas, SLOT(sendSnapshot(bool)));
	connect(_canvas, SIGNAL(newSnapshot(QList<protocol::MessagePtr>)), _client, SLOT(sendSnapshot(QList<protocol::MessagePtr>)));

	// Meta commands
	connect(_client, SIGNAL(chatMessageReceived(QString,QString, bool)),
			chatbox, SLOT(receiveMessage(QString,QString, bool)));
	connect(_client, SIGNAL(chatMessageReceived(QString,QString,bool)),
			this, SLOT(statusbarChat(QString,QString)));
	connect(_client, SIGNAL(markerMessageReceived(QString,QString)),
			chatbox, SLOT(receiveMarker(QString,QString)));
	connect(_client, SIGNAL(markerMessageReceived(QString,QString)),
			this, SLOT(statusbarChat(QString,QString)));
	connect(chatbox, SIGNAL(message(QString)), _client, SLOT(sendChat(QString)));

	connect(_client, SIGNAL(sessionTitleChange(QString)), this, SLOT(setSessionTitle(QString)));
	connect(_client, SIGNAL(opPrivilegeChange(bool)), this, SLOT(setOperatorMode(bool)));
	connect(_client, SIGNAL(sessionConfChange(bool,bool,bool)), this, SLOT(sessionConfChanged(bool,bool,bool)));
	connect(_client, SIGNAL(lockBitsChanged()), this, SLOT(updateLockWidget()));
	connect(_client, SIGNAL(layerVisibilityChange(int,bool)), this, SLOT(updateLockWidget()));

	// Network status changes
	connect(_client, SIGNAL(serverConnected(QString, int)), this, SLOT(connecting()));
	connect(_client, SIGNAL(serverLoggedin(bool)), this, SLOT(loggedin(bool)));
	connect(_client, SIGNAL(serverDisconnected(QString, bool)), this, SLOT(disconnected(QString, bool)));

	connect(_client, SIGNAL(serverConnected(QString, int)), netstatus, SLOT(connectingToHost(QString, int)));
	connect(_client, SIGNAL(serverLoggedin(bool)), netstatus, SLOT(loggedIn()));
	connect(_client, SIGNAL(serverDisconnecting()), netstatus, SLOT(hostDisconnecting()));
	connect(_client, SIGNAL(serverDisconnected(QString, bool)), netstatus, SLOT(hostDisconnected()));
	connect(_client, SIGNAL(expectingBytes(int)),netstatus, SLOT(expectBytes(int)));
	connect(_client, SIGNAL(sendingBytes(int)), netstatus, SLOT(sendingBytes(int)));
	connect(_client, SIGNAL(bytesReceived(int)), netstatus, SLOT(bytesReceived(int)));
	connect(_client, SIGNAL(bytesSent(int)), netstatus, SLOT(bytesSent(int)));

	connect(_client, SIGNAL(userJoined(int, QString)), netstatus, SLOT(join(int, QString)));
	connect(_client, SIGNAL(userLeft(QString)), netstatus, SLOT(leave(QString)));

	connect(_client, SIGNAL(userJoined(int, QString)), chatbox, SLOT(userJoined(int, QString)));
	connect(_client, SIGNAL(userLeft(QString)), chatbox, SLOT(userParted(QString)));

	connect(_client, SIGNAL(userJoined(int,QString)), _canvas, SLOT(setUserMarkerName(int,QString)));

	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateStrokePreviewMode()));
	connect(qApp, SIGNAL(settingsChanged()), this, SLOT(updateShortcuts()));

	updateStrokePreviewMode();

	// Create actions and menus
	setupActions();

	// Restore settings
	readSettings(restoreWindowPosition);
	
	// Set status indicators
	updateLockWidget();
	setRecorderStatus(false);

	// Handle eraser event
	connect(qApp, SIGNAL(eraserNear(bool)), this, SLOT(eraserNear(bool)));

	// Show self
	show();
}

MainWindow::~MainWindow()
{
	// Make sure all child dialogs are closed
	foreach(QObject *obj, children()) {
		QDialog *child = qobject_cast<QDialog*>(obj);
		delete child;
	}
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
	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));

	MainWindow *win;
	if(canReplace()) {
		win = this;
	} else {
		writeSettings();
		win = new MainWindow(false);
	}
	
	QList<protocol::MessagePtr> init = loader.loadInitCommands();

	if(init.isEmpty()) {
		QApplication::restoreOverrideCursor();
		if(win != this)
			delete win;
		showErrorMessage(loader.errorMessage());
		return 0;
	}

	win->_canvas->initCanvas(win->_client);
	win->_dock_layers->init();
	win->_client->init();
	
	// Set local history size limit. This must be at least as big as the initializer,
	// otherwise a new snapshot will always have to be generated when hosting a session.
	uint minsizelimit = 0;
	foreach(protocol::MessagePtr msg, init)
		minsizelimit += msg->length();
	minsizelimit *= 2;

	win->_canvas->statetracker()->setMaxHistorySize(qMax(1024*1024*10u, minsizelimit));
	win->_client->sendLocalInit(init);

	QApplication::restoreOverrideCursor();

	win->_current_filename = loader.filename();
	win->setWindowModified(false);
	win->updateTitle();
	win->_currentdoctools->setEnabled(true);
	win->_docadmintools->setEnabled(true);
	return win;
}

MainWindow *MainWindow::loadRecording(recording::Reader *reader)
{
	MainWindow *win;
	if(canReplace()) {
		win = this;
	} else {
		writeSettings();
		win = new MainWindow(false);
	}

	win->_canvas->initCanvas(win->_client);
	win->_dock_layers->init();
	win->_client->init();

	win->_canvas->statetracker()->setMaxHistorySize(1024*1024*10u);
	win->_canvas->statetracker()->setShowAllUserMarkers(true);

	win->_current_filename = QString();
	win->setWindowModified(false);
	win->updateTitle();
	win->_currentdoctools->setEnabled(true);
	win->_docadmintools->setEnabled(true);

	QFileInfo fileinfo(reader->filename());

	win->_dialog_playback = new dialogs::PlaybackDialog(win->_canvas, reader, win);
	win->_dialog_playback->setWindowTitle(fileinfo.baseName() + " - " + win->_dialog_playback->windowTitle());
	win->_dialog_playback->setAttribute(Qt::WA_DeleteOnClose);

	connect(win->_dialog_playback, &dialogs::PlaybackDialog::commandRead, win->_client, &net::Client::playbackCommand);
	connect(win->_dialog_playback, SIGNAL(playbackToggled(bool)), win, SLOT(setRecorderStatus(bool))); // note: the argument goes unused in this case
	connect(win->_dialog_playback, &dialogs::PlaybackDialog::destroyed, [win]() {
		win->_dialog_playback = 0;
		win->getAction("recordsession")->setEnabled(true);
		win->setRecorderStatus(false);
		win->_canvas->statetracker()->setShowAllUserMarkers(false);
		win->_client->endPlayback();
		win->_canvas->statetracker()->endPlayback();
	});

	win->_dialog_playback->show();
	win->_dialog_playback->centerOnParent();

	win->getAction("recordsession")->setEnabled(false);
	win->setRecorderStatus(false);

	return win;
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
		setWindowTitle(tr("%1[*] - DrawPile").arg(name));
	else
		setWindowTitle(tr("%1[*] - %2 - DrawPile").arg(name).arg(_canvas->title()));
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
			// First reset to defaults
			foreach(QAction *a, win->_customizable_actions)
				a->setShortcut(
						a->property("defaultshortcut").value<QKeySequence>()
						);
			// Then reload customized
			win->loadShortcuts();
		}
	}
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

	cfg.endGroup();
	cfg.beginGroup("tools");
	// Remember last used tool
	int tool = cfg.value("tool", 0).toInt();
	QList<QAction*> actions = _drawingtools->actions();
	if(tool<0 || tool>=actions.count()) tool=0;
	actions[tool]->trigger();
	_dock_toolsettings->setTool(tools::Type(tool));

	// Remember cursor settings
	bool brushoutline = cfg.value("outline",true).toBool();
	getAction("brushoutline")->setChecked(brushoutline);
	_view->setOutline(brushoutline);

	// Remember foreground and background colors
	_fgbgcolor->setForeground(QColor(cfg.value("foreground", "black").toString()));
	_fgbgcolor->setBackground(QColor(cfg.value("background", "white").toString()));

	cfg.endGroup();

	// Customize shortcuts
	loadShortcuts();

	// Remember recent files
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

	cfg.endGroup();
	cfg.beginGroup("tools");
	const int tool = _drawingtools->actions().indexOf(_drawingtools->checkedAction());
	cfg.setValue("tool", tool);
	cfg.setValue("outline", getAction("brushoutline")->isChecked());
	cfg.setValue("foreground", _fgbgcolor->foreground().name());
	cfg.setValue("background", _fgbgcolor->background().name());
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
				tr("Exit DrawPile"),
				tr("You are still connected to a drawing session."),
				QMessageBox::NoButton, this);

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
			QMessageBox box(QMessageBox::Question, tr("Exit DrawPile"),
					tr("There are unsaved changes. Save them before exiting?"),
					QMessageBox::NoButton, this);
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

/**
 * Show the "new document" dialog
 */
void MainWindow::showNew()
{
	auto dlg = new dialogs::NewDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, &dialogs::NewDialog::accepted, [this](const QSize &size, const QColor &background) {
		BlankCanvasLoader bcl(size, background);
		loadDocument(bcl);
	});

	if (_canvas->hasImage())
		dlg->setSize(QSize(_canvas->width(), _canvas->height()));
	else
		dlg->setSize(QSize(800, 600));

	dlg->setBackground(_fgbgcolor->background());
	dlg->show();
}

/**
 * Open the selected file
 * @param file file to open
 * @pre file.isEmpty()!=false
 */
void MainWindow::open(const QString& file)
{
	if(file.endsWith(".dprec", Qt::CaseInsensitive)) {
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
}

/**
 * Show a file selector dialog. If there are unsaved changes, open the file
 * in a new window.
 */
void MainWindow::open()
{
	// Get a list of supported formats
	QString dpimages = "*.ora ";
	QString dprecs = "*.dptxt *.dprec ";
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	const QString filter =
			tr("All supported files (%1)").arg(dpimages + dprecs + formats) + ";;" +
			tr("Images (%1)").arg(dpimages + formats) + ";;" +
			tr("Drawpile recordings (%1)").arg(dprecs) + ";;" +
			tr("All files (*)");

	// Get the file name to open
	const QString file = QFileDialog::getOpenFileName(this,
			tr("Open image"), getLastPath(), filter);

	// Open the file if it was selected
	if(file.isEmpty()==false) {
		const QFileInfo info(file);
		setLastPath(info.absolutePath());

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
	QMessageBox box(QMessageBox::Information, tr("Save image"),
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
	QFileInfo filename(_current_filename);
	if(_current_filename.isEmpty()) {
		return saveas();
	} else {
		QString suffix = QFileInfo(_current_filename).suffix().toLower();

		// Check if suffix is one of the supported formats
		// If not, we need to ask for a new file name
		if(suffix != "ora" && suffix != "png" && suffix != "jpeg" && suffix != "jpg" && suffix!="bmp") {
			return saveas();
		}

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
	QString filter;
#if 0
	// Get a list of all supported formats
	foreach(QByteArray format, QImageWriter::supportedImageFormats()) {
		filter += QString(format).toUpper() + " (*." + format + ");;";
	}
#else
	// We build the filter manually, because these are pretty much the only
	// reasonable formats (who would want to save a 1600x1200 image
	// as an XPM?). Perhaps we should check GIF support was compiled in?
	filter = "OpenRaster (*.ora);;PNG (*.png);;JPEG (*.jpeg);;BMP (*.bmp);;";
	filter += tr("All files (*)");
#endif

	// Get the file name
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save image"), getLastPath(), filter, &selfilter);

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

void MainWindow::setRecorderStatus(bool on)
{
	if(_dialog_playback) {
		if(_dialog_playback->isPlaying()) {
			_recorderstatus->setPixmap(QIcon::fromTheme("media-playback-start", QIcon(":icons/media-playback-start")).pixmap(16, 16));
			_recorderstatus->setToolTip("Playing back recording");
		} else {
			_recorderstatus->setPixmap(QIcon::fromTheme("media-playback-pause", QIcon(":icons/media-playback-pause")).pixmap(16, 16));
			_recorderstatus->setToolTip("Playback paused");
		}
	} else {
		QIcon icon = QIcon::fromTheme("media-record", QIcon(":icons/media-record.png"));
		_recorderstatus->setPixmap(icon.pixmap(16, 16, on ? QIcon::Normal : QIcon::Disabled));
		if(on)
			_recorderstatus->setToolTip("Recording session");
		else
			_recorderstatus->setToolTip("Not recording");

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
		recordAction->setIcon(QIcon::fromTheme("media-record", QIcon(":/icons/media-record")));
		setRecorderStatus(false);
		return;
	}

	QString filter = tr("Drawpile recordings (%1)").arg("*.dprec") + ";;" + tr("All files (*)");
	QString file = QFileDialog::getSaveFileName(this,
			tr("Record session"), getLastPath(), filter);

	if(!file.isEmpty()) {
		// Set file suffix if missing
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

			recordAction->setText("Stop recording");
			recordAction->setIcon(QIcon::fromTheme("media-playback-stop"));

			QApplication::restoreOverrideCursor();
			setRecorderStatus(true);
		}
	}
}

void MainWindow::statusbarChat(const QString &nick, const QString &msg)
{
	// Show message only if chat box is hidden
	if(_splitter->sizes().at(1) == 0)
		statusBar()->showMessage(nick + ": " + msg, 3000);
}

/**
 * The settings window will be window modal and automatically destruct
 * when it is closed.
 */
void MainWindow::showSettings()
{
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog(_customizable_actions, this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->show();
}

void MainWindow::host()
{
	auto dlg = new dialogs::HostDialog(_canvas->image(), this);
	connect(dlg, &dialogs::HostDialog::finished, [this, dlg](int i) {
		if(i==QDialog::Accepted) {
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

			// Remember some settings
			dlg->rememberSettings();

			// Start server if hosting locally
			if(useremote==false) {
				net::ServerThread *server = new net::ServerThread;
				server->setDeleteOnExit();

				QSettings cfg;
				if(cfg.contains("settings/server/port"))
					server->setPort(cfg.value("settings/server/port").toInt());

				if(cfg.value("settings/server/historylimit",false).toBool())
					server->setHistorylimit(cfg.value("settings/server/historysize", 10).toDouble() * 1024*1024);

				int port = server->startServer();
				if(!port) {
					QMessageBox::warning(this, tr("Unable to start server"), tr("An error occurred while trying to start the server"));
					dlg->show();
					delete server;
					return;
				}

				if(!server->isOnDefaultPort())
					address.setPort(port);
			}

			// Initialize session (unless original was used)
			MainWindow *w = this;
			if(dlg->useOriginalImage() == false) {
				QScopedPointer<SessionLoader> loader(dlg->getSessionLoader());
				w = loadDocument(*loader);
			}

			// Connect to server
			net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::HOST, address, w);
			login->setPassword(dlg->getPassword());
			login->setTitle(dlg->getTitle());
			login->setMaxUsers(dlg->getUserLimit());
			login->setAllowDrawing(dlg->getAllowDrawing());
			login->setLayerControlLock(dlg->getLayerControlLock());
			login->setPersistentSessions(dlg->getPersistentMode());
			w->_client->connectToServer(login);

		}
		dlg->deleteLater();
	});
	dlg->show();
}

/**
 * Show the join dialog
 */
void MainWindow::join()
{
	auto dlg = new dialogs::JoinDialog(this);
	connect(dlg, &dialogs::JoinDialog::finished, [this, dlg](int i) {
		if(i==QDialog::Accepted) {
			QString scheme;
			if(dlg->getAddress().startsWith("drawpile://")==false)
				scheme = "drawpile://";
			QUrl address = QUrl(scheme + dlg->getAddress(),QUrl::TolerantMode);
			if(address.isValid()==false || address.host().isEmpty()) {
				dlg->show();
				showErrorMessage(tr("Invalid address"));
				return;
			}
			address.setUserName(dlg->getUserName());

			// Remember some settings
			dlg->rememberSettings();

			// Connect
			joinSession(address);
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
		_canvas->title().isEmpty()?tr("Untitled session"):_canvas->title(),
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
				tr("Session title"),
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
void MainWindow::joinSession(const QUrl& url)
{
	MainWindow *win;
	if(canReplace())
		win = this;
	else
		win = new MainWindow(false);

	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::JOIN, url, win);
	win->_client->connectToServer(login);
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
	_drawingtools->setEnabled(false);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::disconnected(const QString &message, bool localDisconnect)
{
	getAction("hostsession")->setEnabled(true);
	getAction("leavesession")->setEnabled(false);
	_admintools->setEnabled(false);
	_docadmintools->setEnabled(true);

	// Re-enable UI
	_view->setEnabled(true);
	_drawingtools->setEnabled(true);

	setSessionTitle(QString());

	// Display login error if not yet logged in
	if(!_client->isLoggedIn() && !localDisconnect) {
		showErrorMessage(tr("Couldn't connect to server"), message);
	}

	// Make sure all drawing is complete
	if(_canvas->hasImage())
		_canvas->statetracker()->endRemoteContexts();

	updateStrokePreviewMode();
}

/**
 * Server connection established and login successfull
 */
void MainWindow::loggedin(bool join)
{
	// Re-enable UI
	_view->setEnabled(true);
	_drawingtools->setEnabled(true);

	// Initialize the canvas (in host mode the canvas was prepared already)
	if(join) {
		_canvas->initCanvas(_client);
		_dock_layers->init();
		_currentdoctools->setEnabled(true);
	}

	updateStrokePreviewMode();
}

void MainWindow::updateStrokePreviewMode()
{
	drawingboard::StrokePreviewer *preview;
	if(_client->isLocalServer()) {
		preview = drawingboard::NopStrokePreviewer::getInstance();
	} else {
		QSettings cfg;
		int mode = cfg.value("settings/lag/previewstyle", 2).toInt();

		switch(mode) {
		case 0: preview = drawingboard::NopStrokePreviewer::getInstance(); break;
		case 1: preview = new drawingboard::OverlayStrokePreviewer(_canvas); break;
		case 3: preview = new drawingboard::TempLayerStrokePreviewer(_canvas); break;
		default: preview = new drawingboard::ApproximateOverlayStrokePreviewer(_canvas); break;
		}
	}
	_canvas->setStrokePreview(preview);
}

void MainWindow::sessionConfChanged(bool locked, bool layerctrllocked, bool closed)
{
	getAction("locksession")->setChecked(locked);
	getAction("locklayerctrl")->setChecked(layerctrllocked);
	getAction("denyjoins")->setChecked(closed);
	_dock_layers->setControlsLocked(layerctrllocked);
}

void MainWindow::updateLockWidget()
{
	bool locked = _client->isLocked() || _dock_layers->isCurrentLayerLocked();
	if(locked) {
		_lockstatus->setPixmap(QPixmap(":icons/lock_closed.png"));
		_lockstatus->setToolTip(tr("Board is locked"));
	} else {
		_lockstatus->setPixmap(QPixmap(":icons/lock_open.png"));
		_lockstatus->setToolTip(tr("Board is not locked"));
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
		fullscreen(false);
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
		QString("DrawPile"),
		message, QMessageBox::Ok,
		this,
		Qt::Dialog|Qt::Sheet|Qt::MSWindowsFixedSizeDialogHint
	);
	msgbox->setAttribute(Qt::WA_DeleteOnClose);
	msgbox->setWindowModality(Qt::WindowModal);
	msgbox->setDetailedText(details);
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
		// lasttool might be erasertool when tablet is brought near
		if(_lasttool == annotationtool)
			_lasttool = getAction("toolbrush");
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
		// lasttool might be erasertool when tablet is brought near
		if(_lasttool == lasertool)
			_lasttool = getAction("toolbrush");
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
void MainWindow::fullscreen(bool enable)
{
	if(enable) {
		Q_ASSERT(windowState().testFlag(Qt::WindowFullScreen)==false);
		// Save state
		_fullscreen_oldstate = saveState();
		_fullscreen_oldgeometry = geometry();

		// Hide everything except floating docks
		menuBar()->hide();
		statusBar()->hide();
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
		Q_ASSERT(windowState().testFlag(Qt::WindowFullScreen)==true);
		// Restore old state
		showNormal();
		menuBar()->show();
		statusBar()->show();
		setGeometry(_fullscreen_oldgeometry);
		restoreState(_fullscreen_oldstate);
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

	tools::Type type = tools::Type(idx);
	_lasttool = tool;

	// When using the annotation tool, highlight all text boxes
	_canvas->showAnnotationBorders(type==tools::ANNOTATION);

	// Send pointer updates when using the laser pointer (TODO checkbox)
	_view->setPointerTracking(type==tools::LASERPOINTER && _dock_toolsettings->getLaserPointerSettings()->pointerTracking());

	// Remove selection when not using selection tool
	if(type != tools::SELECTION)
		_canvas->setSelectionItem(0);

	emit toolChanged(type);
}

/**
 * When the eraser is near, switch to eraser tool. When not, switch to
 * whatever tool we were using before
 * @param near
 */
void MainWindow::eraserNear(bool near)
{
	if(near) {
		QAction *lt = _lasttool; // Save _lasttool
		getAction("tooleraser")->trigger();
		_lasttool = lt;
	} else {
		_lasttool->trigger();
	}
}

void MainWindow::selectAll()
{
	getAction("toolselectrect")->trigger();
	_canvas->setSelectionItem(QRect(0, 0, _canvas->width(), _canvas->height()));
}

void MainWindow::selectNone()
{
	_canvas->setSelectionItem(0);
}

void MainWindow::cutLayer()
{
	QImage img = _canvas->selectionToImage(_dock_layers->currentLayer());
	clearArea();
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
	const QString filter = tr("Images (%1)").arg(formats) + ";;" + tr("All files (*)");

	// Get the file name to open
	const QString file = QFileDialog::getOpenFileName(this,
			tr("Paste image"), getLastPath(), filter);

	// Open the file if it was selected
	if(file.isEmpty()==false) {
		const QFileInfo info(file);
		setLastPath(info.absolutePath());

		pasteFile(QUrl::fromLocalFile(file));
	}
}

void MainWindow::pasteFile(const QUrl &url)
{
	// TODO load remote URLs
	if(!url.isLocalFile())
		return;

	QImage img(url.toLocalFile());
	if(img.isNull()) {
		showErrorMessage(tr("The image could not be loaded"));
		return;
	}

	pasteImage(img);
}

void MainWindow::pasteImage(const QImage &image)
{
	if(_canvas->hasImage()) {
		getAction("toolselectrect")->trigger();
		_canvas->pasteFromImage(image, _view->viewCenterPoint());
	} else {
		// Canvas not yet initialized? Initialize with clipboard content
		QImageCanvasLoader loader(image);
		loadDocument(loader);
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

void MainWindow::clearArea()
{
	fillArea(Qt::transparent);
}

void MainWindow::fillFgArea()
{
	fillArea(_fgbgcolor->foreground());
}

void MainWindow::fillBgArea()
{
	fillArea(_fgbgcolor->background());
}

void MainWindow::fillArea(const QColor &color)
{
	const QRect bounds = QRect(0, 0, _canvas->width(), _canvas->height());
	QRect area;
	if(_canvas->selectionItem())
		area = _canvas->selectionItem()->rect().intersected(bounds);
	else
		area = bounds;

	if(!area.isEmpty()) {
		_client->sendUndopoint();
		_client->sendFillRect(_dock_layers->currentLayer(), area, color);
	}
}

void MainWindow::resizeCanvas()
{
	QSize size(_canvas->width(), _canvas->height());
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
	QString text = QInputDialog::getText(this, tr("Mark position"), tr("Marker text"), QLineEdit::Normal, QString(), &ok);
	if(ok)
		_client->sendMarker(text);
}

void MainWindow::about()
{
	QMessageBox::about(this, tr("About Drawpile"),
			tr("<p><b>Drawpile %1</b><br>"
			"A collaborative drawing program.</p>"

			"<p>Copyright © 2007-2014 Calle Laakkonen</p>"

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
			).arg(DRAWPILE_VERSION)
	);
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
		qicon = QIcon::fromTheme(icon, QIcon(QLatin1Literal(":icons/") + icon));
	act = new QAction(qicon, text, this);
	if(name)
		act->setObjectName(name);
	if(shortcut.isEmpty()==false) {
		act->setShortcut(shortcut);
		act->setProperty("defaultshortcut", shortcut);
	}

	act->setCheckable(checkable);

	if(tip.isEmpty()==false)
		act->setStatusTip(tip);

	if(name!=0 && name[0]!='\0')
		_customizable_actions.append(act);

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
	QAction *save = makeAction("savedocument", "document-save",tr("&Save"), QString(),QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", "document-save-as", tr("Save &As..."));
	QAction *record = makeAction("recordsession", "media-record", tr("Record..."));
	QAction *quit = makeAction("exitprogram", "application-exit", tr("&Quit"), QString(), QKeySequence("Ctrl+Q"));
	quit->setMenuRole(QAction::QuitRole);

	_currentdoctools->addAction(save);
	_currentdoctools->addAction(saveas);
	_currentdoctools->addAction(record);

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));
	connect(record, SIGNAL(triggered()), this, SLOT(toggleRecording()));
	connect(quit, SIGNAL(triggered()), this, SLOT(close()));

	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(newdocument);
	filemenu->addAction(open);
	_recent = filemenu->addMenu(tr("Open &recent"));
	filemenu->addAction(save);
	filemenu->addAction(saveas);
	filemenu->addSeparator();
	filemenu->addAction(record);
	filemenu->addSeparator();
	filemenu->addAction(quit);

	QToolBar *filetools = new QToolBar(tr("File tools"));
	filetools->setObjectName("filetoolsbar");
	toggletoolbarmenu->addAction(filetools->toggleViewAction());
	filetools->addAction(newdocument);
	filetools->addAction(open);
	filetools->addAction(save);
	filetools->addAction(saveas);
	addToolBar(Qt::TopToolBarArea, filetools);

	connect(_recent, &QMenu::triggered, [this](QAction *action) {
		action->setProperty("deletelater",true);
		this->open(action->property("filepath").toString());
	});

	//
	// Edit menu
	//
	QAction *undo = makeAction("undo", "edit-undo", tr("&Undo"), QString(), QKeySequence::Undo);
	QAction *redo = makeAction("redo", "edit-redo", tr("&Redo"), QString(), QKeySequence::Redo);
	QAction *copy = makeAction("copyvisible", "edit-copy", tr("&Copy visible"), tr("Copy selected area to the clipboard"), QKeySequence::Copy);
	QAction *copylayer = makeAction("copylayer", "edit-copy", tr("Copy &layer"), tr("Copy selected area of the current layer to the clipboard"));
	QAction *cutlayer = makeAction("cutlayer", "edit-cut", tr("Cu&t layer"), tr("Cut selected area of the current layer to the clipboard"), QKeySequence::Cut);
	QAction *paste = makeAction("paste", "edit-paste", tr("&Paste"), QString(), QKeySequence::Paste);
	QAction *pastefile = makeAction("pastefile", "document-open", tr("Paste &from file..."));
	QAction *deleteAnnotations = makeAction("deleteemptyannotations", 0, tr("Delete empty annotations"));
	QAction *resize = makeAction("resizecanvas", 0, tr("Resi&ze canvas..."));
	QAction *preferences = makeAction(0, 0, tr("Prefere&nces..."));

	QAction *selectall = makeAction("selectall", 0, tr("Select &all"), QString(), QKeySequence::SelectAll);
#if (QT_VERSION < QT_VERSION_CHECK(5, 1, 0))
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"));
#else
	QAction *selectnone = makeAction("selectnone", 0, tr("&Deselect"), QString(), QKeySequence::Deselect);
#endif

	QAction *expandup = makeAction("expandup", 0, tr("Expand up"), "", QKeySequence("Ctrl+J"));
	QAction *expanddown = makeAction("expandup", 0, tr("Expand down"), "", QKeySequence("Ctrl+K"));
	QAction *expandleft = makeAction("expandup", 0, tr("Expand left"), "", QKeySequence("Ctrl+H"));
	QAction *expandright = makeAction("expandup", 0, tr("Expand right"), "", QKeySequence("Ctrl+L"));

	QAction *cleararea = makeAction("cleararea", 0, tr("Clear"), tr("Clear selected area of the current layer"), QKeySequence("Delete"));
	QAction *fillfgarea = makeAction("fillfgarea", 0, tr("Fill with &FG color"), tr("Fill selected area with foreground color"), QKeySequence("Ctrl+,"));
	QAction *fillbgarea = makeAction("fillbgarea", 0, tr("Fill with B&G color"), tr("Fill selected area with background color"), QKeySequence("Ctrl+."));

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
	connect(cleararea, SIGNAL(triggered()), this, SLOT(clearArea()));
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
	QMenu *expandmenu = editmenu->addMenu(tr("Expand canvas"));
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

	//
	// View menu
	//
	QAction *toolbartoggles = new QAction(tr("&Toolbars"), this);
	toolbartoggles->setMenu(toggletoolbarmenu);

	QAction *docktoggles = new QAction(tr("&Docks"), this);
	docktoggles->setMenu(toggledockmenu);

	QAction *zoomin = makeAction("zoomin", "zoom-in",tr("Zoom &in"), QString(), QKeySequence::ZoomIn);
	QAction *zoomout = makeAction("zoomout", "zoom-out",tr("Zoom &out"), QString(), QKeySequence::ZoomOut);
	QAction *zoomorig = makeAction("zoomone", "zoom-original",tr("&Normal size"), QString(), QKeySequence(Qt::CTRL + Qt::Key_0));
	QAction *rotateorig = makeAction("rotatezero", "view-refresh",tr("&Reset rotation"), tr("Drag the view while holding ctrl-space to rotate"), QKeySequence(Qt::CTRL + Qt::Key_R));
	QAction *rotate90 = makeAction("rotate90", 0, tr("Rotate to 90°"));
	QAction *rotate180 = makeAction("rotate180", 0, tr("Rotate to 180°"));
	QAction *rotate270 = makeAction("rotate270", 0, tr("Rotate to 270°"));

	QAction *showoutline = makeAction("brushoutline", 0, tr("Show brush &outline"), QString(), QKeySequence(), true);
	QAction *showannotations = makeAction("showannotations", 0, tr("Show &annotations"), QString(), QKeySequence(), true);
	QAction *showusermarkers = makeAction("showusermarkers", 0, tr("Show user pointers"), QString(), QKeySequence(), true);
	QAction *showlasers = makeAction("showlasers", 0, tr("Show laser trails"), QString(), QKeySequence(), true);
	showannotations->setChecked(true);
	showusermarkers->setChecked(true);
	showlasers->setChecked(true);

	QAction *fullscreen = makeAction("fullscreen", 0, tr("&Full screen"), QString(), QKeySequence("F11"), true);

	connect(zoomin, SIGNAL(triggered()), _view, SLOT(zoomin()));
	connect(zoomout, SIGNAL(triggered()), _view, SLOT(zoomout()));
	connect(zoomorig, &QAction::triggered, [this]() { _view->setZoom(100.0); });
	connect(rotateorig, &QAction::triggered, [this]() { _view->setRotation(0); });
	connect(rotate90, &QAction::triggered, [this]() { _view->setRotation(90); });
	connect(rotate180, &QAction::triggered, [this]() { _view->setRotation(180); });
	connect(rotate270, &QAction::triggered, [this]() { _view->setRotation(270); });
	connect(fullscreen, SIGNAL(triggered(bool)), this, SLOT(fullscreen(bool)));

	connect(showoutline, SIGNAL(triggered(bool)), _view, SLOT(setOutline(bool)));
	connect(showannotations, SIGNAL(triggered(bool)), this, SLOT(setShowAnnotations(bool)));
	connect(showusermarkers, SIGNAL(triggered(bool)), _canvas, SLOT(showUserMarkers(bool)));
	connect(showlasers, SIGNAL(triggered(bool)), this, SLOT(setShowLaserTrails(bool)));

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(toolbartoggles);
	viewmenu->addAction(docktoggles);
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

	viewmenu->addSeparator();
	viewmenu->addAction(showoutline);
	viewmenu->addAction(showannotations);
	viewmenu->addAction(showusermarkers);
	viewmenu->addAction(showlasers);

	viewmenu->addSeparator();
	viewmenu->addAction(fullscreen);

	//
	// Session menu
	//
	QAction *host = makeAction("hostsession", 0, tr("&Host..."),tr("Share your drawingboard with others"));
	QAction *join = makeAction("joinsession", 0, tr("&Join..."),tr("Join another user's drawing session"));
	QAction *logout = makeAction("leavesession", 0, tr("&Leave"),tr("Leave this drawing session"));
	logout->setEnabled(false);

	QAction *locksession = makeAction("locksession", 0, tr("Lo&ck the board"), tr("Prevent changes to the drawing board"), QKeySequence("Ctrl+L"), true);
	QAction *locklayerctrl = makeAction("locklayerctrl", 0, tr("Lock layer controls"), tr("Allow only session operators to add and change layers"), QKeySequence(), true);
	QAction *closesession = makeAction("denyjoins", 0, tr("&Deny joins"), tr("Prevent new users from joining the session"), QKeySequence(), true);

	QAction *changetitle = makeAction("changetitle", 0, tr("Change &title..."));

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
	QAction *selectiontool = makeAction("toolselectrect", "select-rectangular", tr("&Select"), tr("Select area for copying"), QKeySequence("S"), true);
	QAction *pentool = makeAction("toolpen", "draw-freehand", tr("&Pen"), tr("Draw with hard edged strokes"), QKeySequence("P"), true);
	QAction *brushtool = makeAction("toolbrush", "draw-brush", tr("&Brush"), tr("Draw with smooth strokes"), QKeySequence("B"), true);
	QAction *erasertool = makeAction("tooleraser", "draw-eraser", tr("&Eraser"), tr("Erase layer content"), QKeySequence("E"), true);
	QAction *pickertool = makeAction("toolpicker", "color-picker", tr("&Color picker"), tr("Pick colors from the image"), QKeySequence("I"), true);
	QAction *linetool = makeAction("toolline", "draw-line", tr("&Line"), tr("Draw straight lines"), QKeySequence("U"), true);
	QAction *recttool = makeAction("toolrect", "draw-rectangle", tr("&Rectangle"), tr("Draw unfilled squares and rectangles"), QKeySequence("R"), true);
	QAction *ellipsetool = makeAction("toolellipse", "draw-ellipse", tr("&Ellipse"), tr("Draw unfilled circles and ellipses"), QKeySequence("O"), true);
	QAction *annotationtool = makeAction("tooltext", "draw-text", tr("&Annotation"), tr("Add text to the picture"), QKeySequence("A"), true);
	QAction *lasertool = makeAction("toollaser", "tool-laserpointer", tr("&Laser pointer"), tr("Point out things on the canvas"), QKeySequence("L"), true);
	QAction *markertool = makeAction("toolmarker", "flag-red", tr("&Mark"), tr("Leave a marker to find this spot on the recording"), QKeySequence("Ctrl+M"));

	connect(markertool, SIGNAL(triggered()), this, SLOT(markSpotForRecording()));

	// Default tool
	brushtool->setChecked(true);
	_lasttool = brushtool;

	_drawingtools->addAction(selectiontool);
	_drawingtools->addAction(pentool);
	_drawingtools->addAction(brushtool);
	_drawingtools->addAction(erasertool);
	_drawingtools->addAction(pickertool);
	_drawingtools->addAction(linetool);
	_drawingtools->addAction(recttool);
	_drawingtools->addAction(ellipsetool);
	_drawingtools->addAction(annotationtool);
	_drawingtools->addAction(lasertool);

	connect(_drawingtools, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(_drawingtools->actions());
	toolsmenu->addAction(markertool);
	toolsmenu->addSeparator();

	QMenu *toolshortcuts = toolsmenu->addMenu(tr("&Shortcuts"));

	QAction *swapcolors = makeAction("swapcolors", 0, tr("&Swap colors"), QString(), QKeySequence(Qt::Key_X));
	QAction *smallerbrush = makeAction("ensmallenbrush", 0, tr("&Decrease brush size"), QString(), Qt::Key_BracketLeft);
	QAction *biggerbrush = makeAction("embiggenbrush", 0, tr("&Increase brush size"), QString(), Qt::Key_BracketRight);

	connect(smallerbrush, &QAction::triggered, [this]() { _view->doQuickAdjust1(-1);});
	connect(biggerbrush, &QAction::triggered, [this]() { _view->doQuickAdjust1(1);});

	toolshortcuts->addAction(smallerbrush);
	toolshortcuts->addAction(biggerbrush);
	toolshortcuts->addAction(swapcolors);

	QToolBar *drawtools = new QToolBar("Drawing tools");
	drawtools->setObjectName("drawtoolsbar");
	toggletoolbarmenu->addAction(drawtools->toggleViewAction());

	drawtools->addActions(_drawingtools->actions());
	drawtools->addSeparator();
	drawtools->addAction(zoomin);
	drawtools->addAction(zoomout);
	drawtools->addAction(zoomorig);
	drawtools->addAction(rotateorig);
	drawtools->addSeparator();

	// Create color button for drawing tool bar
	_fgbgcolor = new widgets::DualColorButton(drawtools);

	connect(swapcolors, SIGNAL(triggered()), _fgbgcolor, SLOT(swapColors()));
	connect(_fgbgcolor, SIGNAL(foregroundChanged(const QColor&)), _dock_toolsettings, SLOT(setForeground(const QColor&)));
	connect(_fgbgcolor, SIGNAL(backgroundChanged(const QColor&)), _dock_toolsettings, SLOT(setBackground(const QColor&)));
	connect(_view, SIGNAL(colorDropped(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_dock_toolsettings->getColorPickerSettings(), SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	connect(_canvas, &drawingboard::CanvasScene::colorPicked, [this](const QColor &c, bool bg) {
		if(bg)
			_fgbgcolor->setBackground(c);
		else
			_fgbgcolor->setForeground(c);
	});

	connect(_dock_palette, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundChanged(QColor)), _dock_rgb, SLOT(setColor(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundChanged(QColor)), _dock_hsv, SLOT(setColor(QColor)));

	connect(_dock_rgb, SIGNAL(colorChanged(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_dock_hsv, SIGNAL(colorChanged(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	// Create color changer dialogs
	auto dlg_fgcolor = new dialogs::ColorDialog(this, tr("Foreground color"));
	connect(dlg_fgcolor, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundClicked(QColor)), dlg_fgcolor, SLOT(pickNewColor(QColor)));

	auto dlg_bgcolor = new dialogs::ColorDialog(this, tr("Background color"));
	connect(dlg_bgcolor, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setBackground(QColor)));
	connect(_fgbgcolor, SIGNAL(backgroundClicked(QColor)), dlg_bgcolor, SLOT(pickNewColor(QColor)));

	drawtools->addWidget(_fgbgcolor);

	addToolBar(Qt::TopToolBarArea, drawtools);

	//
	// Help menu
	//
	QAction *homepage = makeAction("dphomepage", 0, tr("&Homepage"), "http://drawpile.sourceforge.net/");
	QAction *wikipage = makeAction("dphomepage", 0, tr("&Wiki"), "https://github.com/callaa/Drawpile/wiki");
	QAction *about = makeAction("dpabout", 0, tr("&About DrawPile"));
	QAction *aboutqt = makeAction("aboutqt", 0, tr("About &Qt"));

	connect(homepage, &QAction::triggered, [homepage]() { QDesktopServices::openUrl(QUrl(homepage->statusTip())); });
	connect(wikipage, &QAction::triggered, [wikipage]() { QDesktopServices::openUrl(QUrl(wikipage->statusTip())); });
	connect(about, SIGNAL(triggered()), this, SLOT(about()));
	connect(aboutqt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addAction(wikipage);
	helpmenu->addSeparator();
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);
}

void MainWindow::createDocks()
{
	// Create tool settings
	_dock_toolsettings = new docks::ToolSettings(this);
	_dock_toolsettings->setObjectName("ToolSettings");
	_dock_toolsettings->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	connect(this, SIGNAL(toolChanged(tools::Type)), _dock_toolsettings, SLOT(setTool(tools::Type)));
	addDockWidget(Qt::RightDockWidgetArea, _dock_toolsettings);

	// Create input settings
	_dock_input = new docks::InputSettings(this);
	_dock_input->setObjectName("InputSettings");
	_dock_input->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _dock_input);

	// Create color boxes
	_dock_rgb = new docks::ColorBox("RGB", docks::ColorBox::RGB, this);
	_dock_rgb->setObjectName("rgbdock");

	_dock_hsv = new docks::ColorBox("HSV", docks::ColorBox::HSV, this);
	_dock_hsv->setObjectName("hsvdock");

	addDockWidget(Qt::RightDockWidgetArea, _dock_rgb);
	addDockWidget(Qt::RightDockWidgetArea, _dock_hsv);

	// Create palette box
	_dock_palette = new docks::PaletteBox(tr("Palette"), this);
	_dock_palette->setObjectName("palettedock");
	addDockWidget(Qt::RightDockWidgetArea, _dock_palette);

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

	// Tabify docks
	tabifyDockWidget(_dock_input, _dock_toolsettings);
	tabifyDockWidget(_dock_hsv, _dock_rgb);
	tabifyDockWidget(_dock_hsv, _dock_palette);
	tabifyDockWidget(_dock_users, _dock_layers);
}
