/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <QCloseEvent>
#include <QPushButton>
#include <QImageReader>
#include <QImageWriter>
#include <QSplitter>
#include <QTemporaryFile>
#include <QTimer>

#include "main.h"
#include "mainwindow.h"
#include "localserver.h"
#include "icons.h"
#include "version.h"
#include "loader.h"

#include "canvasview.h"
#include "canvasscene.h"

#include "utils/recentfiles.h"

#include "widgets/viewstatus.h"
#include "widgets/userlistwidget.h"
#include "widgets/netstatus.h"
#include "widgets/dualcolorbutton.h"
#include "widgets/chatwidget.h"
#include "widgets/layerlistwidget.h"

#include "docks/toolsettingswidget.h"
#include "docks/palettebox.h"
#include "docks/navigator.h"
#include "docks/colorbox.h"

#include "net/client.h"

#include "dialogs/colordialog.h"
#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/settingsdialog.h"

/**
 * @param source if not null, clone settings from this window
 */
MainWindow::MainWindow(const MainWindow *source)
	: QMainWindow(), _canvas(0)
{
	setTitle();

	initActions();
	createMenus();
	createToolbars();
	createDocks();
	createDialogs();

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	// Create the view status widget
	viewstatus_ = new widgets::ViewStatus(this);
	statusbar->addPermanentWidget(viewstatus_);

	// Create net status widget
	netstatus_ = new widgets::NetStatus(this);
	statusbar->addPermanentWidget(netstatus_);

	// Create lock status widget
	lockstatus_ = new QLabel(this);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
	lockstatus_->setToolTip(tr("Board is not locked"));
	statusbar->addPermanentWidget(lockstatus_);

	// Work area is split between the view and the chatbox
	splitter_ = new QSplitter(Qt::Vertical, this);
	setCentralWidget(splitter_);

	// Create view
	_view = new widgets::CanvasView(this);
	_view->setToolSettings(toolsettings_);
	
	connect(_layerlist, SIGNAL(layerSelected(int)), _view, SLOT(selectLayer(int)));

	splitter_->addWidget(_view);
	splitter_->setCollapsible(0, false);

	connect(toggleoutline_, SIGNAL(triggered(bool)),
			_view, SLOT(setOutline(bool)));
#if 0
	connect(toolsettings_, SIGNAL(sizeChanged(int)),
			_view, SLOT(setOutlineRadius(int)));
	connect(toolsettings_, SIGNAL(colorsChanged(const QColor&, const QColor&)),
			_view, SLOT(setOutlineColors(const QColor&, const QColor&)));
#endif
	connect(_view, SIGNAL(imageDropped(QString)),
			this, SLOT(open(QString)));
	connect(_view, SIGNAL(viewTransformed(int, qreal)),
			viewstatus_, SLOT(setTransformation(int, qreal)));

	connect(this, SIGNAL(toolChanged(tools::Type)), _view, SLOT(selectTool(tools::Type)));
	
	// Create the chatbox
	chatbox_ = new widgets::ChatBox(this);
	splitter_->addWidget(chatbox_);

	// Create board
	_canvas = new drawingboard::CanvasScene(this, _layerlist);
	_canvas->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	_view->setBoard(_canvas);
	navigator_->setScene(_canvas);

	// Navigator <-> View
	connect(navigator_, SIGNAL(focusMoved(const QPoint&)),
			_view, SLOT(scrollTo(const QPoint&)));
	connect(_view, SIGNAL(viewMovedTo(const QRectF&)), navigator_,
			SLOT(setViewFocus(const QRectF&)));
	// Navigator <-> Zoom In/Out
	connect(navigator_, SIGNAL(zoomIn()), this, SLOT(zoomin()));
	connect(navigator_, SIGNAL(zoomOut()), this, SLOT(zoomout()));

	// Create the network client
	_client = new net::Client(this);
	_view->setClient(_client);
	_layerlist->setClient(_client);
	
	// Client command receive signals
	connect(_client, SIGNAL(drawingCommandReceived(protocol::Message*)), _canvas, SLOT(handleDrawingCommand(protocol::Message*)));

#if 0
	controller_ = new Controller(toolsettings_->getAnnotationSettings(), this);
	controller_->setModel(_board);
	connect(controller_, SIGNAL(changed()),
			this, SLOT(boardChanged()));
	connect(this, SIGNAL(toolChanged(tools::Type)),
			controller_, SLOT(setTool(tools::Type)));

	connect(_view,SIGNAL(penDown(dpcore::Point)),
			controller_,SLOT(penDown(dpcore::Point)));
	connect(_view,SIGNAL(penMove(dpcore::Point)),
			controller_,SLOT(penMove(dpcore::Point)));
	connect(_view,SIGNAL(penUp()),
			controller_,SLOT(penUp()));

	// Controller -> netstatus
	connect(controller_, SIGNAL(disconnected(QString)),
			netstatus_, SLOT(disconnectHost()));
	connect(controller_, SIGNAL(connected(const QString&)),
			netstatus_, SLOT(connectHost(const QString&)));
	connect(controller_, SIGNAL(userJoined(network::User)),
			netstatus_, SLOT(join(network::User)));
	connect(controller_, SIGNAL(userParted(network::User)),
			netstatus_, SLOT(leave(network::User)));
	connect(controller_, SIGNAL(userKicked(network::User)),
			netstatus_, SLOT(kicked(network::User)));
	connect(controller_, SIGNAL(lockboard(QString)),
			netstatus_, SLOT(lock(QString)));
	connect(controller_, SIGNAL(unlockboard()),
			netstatus_, SLOT(unlock()));

	// Actions -> controller
	connect(lock_board, SIGNAL(triggered(bool)),
			controller_, SLOT(lockBoard(bool)));
	connect(disallowjoins_, SIGNAL(triggered(bool)),
			controller_, SLOT(disallowJoins(bool)));

	// Controller <-> mainwindow
	connect(controller_, SIGNAL(connected(QString)),
			this, SLOT(connected()));
	connect(controller_, SIGNAL(disconnected(QString)),
			this, SLOT(disconnected()));
	connect(controller_, SIGNAL(lockboard(QString)),
			this, SLOT(lock(QString)));
	connect(controller_, SIGNAL(unlockboard()),
			this, SLOT(unlock()));
	connect(controller_, SIGNAL(joined()),
			this, SLOT(joined()));
	connect(controller_, SIGNAL(boardChanged()),
			this, SLOT(boardInfoChanged()));
	connect(controller_, SIGNAL(rasterUploadProgress(int)),
			this, SLOT(rasterUp(int)));

	// Controller <-> login dialog connections
	connect(controller_, SIGNAL(connected(const QString&)),
			logindlg_, SLOT(connected()));
	connect(controller_, SIGNAL(disconnected(QString)),
			logindlg_, SLOT(disconnected(QString)));
	connect(controller_, SIGNAL(loggedin()), logindlg_,
			SLOT(loggedin()));
	connect(controller_, SIGNAL(rasterDownloadProgress(int)),
			logindlg_, SLOT(raster(int)));
	connect(controller_, SIGNAL(netError(QString)),
			logindlg_, SLOT(error(QString)));
	connect(controller_, SIGNAL(needPassword()),
			logindlg_, SLOT(getPassword()));
	connect(logindlg_, SIGNAL(password(QString)),
			controller_, SLOT(sendPassword(QString)));

	// Chatbox <-> Controller
	connect(controller_, SIGNAL(chat(QString,QString)),
			chatbox_, SLOT(receiveMessage(QString,QString)));
	connect(controller_, SIGNAL(parted()),
			chatbox_, SLOT(parted()));
	connect(chatbox_, SIGNAL(message(QString)),
			controller_, SLOT(sendChat(QString)));
	connect(netstatus_, SIGNAL(statusMessage(QString)),
			chatbox_, SLOT(systemMessage(QString)));

	// Layer box -> controller
	connect(_layerlist, SIGNAL(newLayer(const QString&)),
			controller_, SLOT(newLayer(const QString&)));
	connect(_layerlist, SIGNAL(deleteLayer(int, bool)),
			controller_, SLOT(deleteLayer(int, bool)));
	connect(_layerlist, SIGNAL(layerMove(int, int)),
			controller_, SLOT(moveLayer(int, int)));
	connect(_layerlist, SIGNAL(renameLayer(int, const QString&)),
			controller_, SLOT(renameLayer(int, const QString&)));
	connect(_layerlist, SIGNAL(selected(int)),
			controller_, SLOT(selectLayer(int)));
	connect(_layerlist, SIGNAL(opacityChange(int,int)),
			controller_, SLOT(setLayerOpacity(int,int)));
	connect(_layerlist, SIGNAL(layerToggleHidden(int)),
			controller_, SLOT(toggleLayerHidden(int)));
#endif
	if(source)
		cloneSettings(source);
	else
		readSettings();
	
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
 * If the document in this window cannot be rep
 * @return true on success
 */
bool MainWindow::loadDocument(const SessionLoader &loader)
{
	
	MainWindow *win = canReplace() ? this : new MainWindow(this);
	
	win->_canvas->initCanvas();
	win->_client->init();
	
	bool ok = loader.sendInitCommands(win->_client);
	
	if(!ok) {
		if(win != this)
			delete win;
		showErrorMessage(ERR_OPEN);
		return false;
	}
	
	// TODO filename
	win->filename_ = ""; //filename;
	win->setWindowModified(false);
	win->setTitle();
	win->save_->setEnabled(true);
	win->saveas_->setEnabled(true);
	return true;
}

void MainWindow::initDefaultCanvas()
{
	loadDocument(BlankCanvasLoader(QSize(800, 600), Qt::white));
}


/**
 * @param url URL
 */
void MainWindow::joinSession(const QUrl& url)
{
#if 0
	controller_->joinSession(url);

	// Set login dialog to correct state
	logindlg_->connecting(url.host(), false);
	connect(logindlg_, SIGNAL(rejected()), controller_, SLOT(disconnectHost()));
#endif
}

/**
 * This function is used to check if the current board can be replaced
 * or if a new window is needed to open other content.
 *
 * The window can be replaced when there are no unsaved changes AND the board
 * is not joined to a network session.
 * @retval false if a new window needs to be created
 */
bool MainWindow::canReplace() const {
#if 0 // TODO
	return !(isWindowModified() || _canvas->hasAnnotations() ||
		controller_->isConnected());
#else
	return true;
#endif
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
			RecentFiles::initMenu(win->recent_);
	}
}

/**
 * Set window title according to currently open file and session
 */
void MainWindow::setTitle()
{
	QString name;
	if(filename_.isEmpty()) {
		name = tr("Untitled");
	} else {
		const QFileInfo info(filename_);
		name = info.baseName();
	}

	if(sessiontitle_.isEmpty())
		setWindowTitle(tr("%1[*] - DrawPile").arg(name));
	else
		setWindowTitle(tr("%1[*] - %2 - DrawPile").arg(name).arg(sessiontitle_));
}

/**
 * Load customized shortcuts
 */
void MainWindow::loadShortcuts()
{
	QSettings& cfg = DrawPileApp::getSettings();
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
			foreach(QAction *a, win->customacts_)
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
void MainWindow::readSettings()
{
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("window");

	// Restore previously used window size and position
	resize(cfg.value("size",QSize(800,600)).toSize());

	if(cfg.contains("pos")) {
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
		splitter_->restoreState(cfg.value("viewstate").toByteArray());
	}

	lastpath_ = cfg.value("lastpath").toString();

	cfg.endGroup();
	cfg.beginGroup("tools");
	// Remember last used tool
	int tool = cfg.value("tool", 0).toInt();
	QList<QAction*> actions = drawingtools_->actions();
	if(tool<0 || tool>=actions.count()) tool=0;
	actions[tool]->trigger();
	toolsettings_->setTool(tools::Type(tool));

	// Remember cursor settings
	toggleoutline_->setChecked(cfg.value("outline",true).toBool());
	_view->setOutline(toggleoutline_->isChecked());

	// Remember foreground and background colors
	fgbgcolor_->setForeground(QColor(cfg.value("foreground", "black").toString()));
	fgbgcolor_->setBackground(QColor(cfg.value("background", "white").toString()));

	cfg.endGroup();

	// Customize shortcuts
	loadShortcuts();

	// Remember recent files
	RecentFiles::initMenu(recent_);
}

/**
 * @param source window whose settings are cloned
 */
void MainWindow::cloneSettings(const MainWindow *source)
{
	// Clone size, but let the window manager position this window
	resize(source->normalGeometry().size());
	//source->size() fails miserably if window is maximized
	
	// Clone window state?
	//setWindowState(source->windowState());
	
	// Copy dock and view states
	restoreState(source->saveState());
	splitter_->restoreState(source->splitter_->saveState());

	lastpath_ = source->lastpath_;

	// Copy tool selection
	const int tool = source->drawingtools_->actions().indexOf(
			source->drawingtools_->checkedAction()
			);
	drawingtools_->actions()[tool]->trigger();
	toolsettings_->setTool(tools::Type(tool));
	_view->selectTool(tools::Type(tool));

	// Copy foreground and background colors
	fgbgcolor_->setForeground(source->fgbgcolor_->foreground());
	fgbgcolor_->setBackground(source->fgbgcolor_->background());
}


/**
 * Write out settings
 */
void MainWindow::writeSettings()
{
	QSettings& cfg = DrawPileApp::getSettings();
	cfg.beginGroup("window");
	
	cfg.setValue("pos", normalGeometry().topLeft());
	cfg.setValue("size", normalGeometry().size());
	
	cfg.setValue("maximized", isMaximized());
	cfg.setValue("state", saveState());
	cfg.setValue("viewstate", splitter_->saveState());
	cfg.setValue("lastpath", lastpath_);

	cfg.endGroup();
	cfg.beginGroup("tools");
	const int tool = drawingtools_->actions().indexOf(drawingtools_->checkedAction());
	cfg.setValue("tool", tool);
	cfg.setValue("outline", toggleoutline_->isChecked());
	cfg.setValue("foreground",fgbgcolor_->foreground().name());
	cfg.setValue("background",fgbgcolor_->background().name());
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
#if 0
		if(controller_->isConnected()) {
			QMessageBox box(QMessageBox::Information, tr("Exit DrawPile"),
					controller_->isUploading()?
					tr("You are currently sending board contents to a new user. Please wait until it has been fully sent."):
					tr("You are still connected to a drawing session."),
					QMessageBox::NoButton, this);

			const QPushButton *exitbtn = box.addButton(tr("Exit anyway"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Cancel"),
					QMessageBox::RejectRole);

			box.exec();
			if(box.clickedButton() == exitbtn) {
				// Delay exiting until actually disconnected
				connect(controller_, SIGNAL(disconnected(QString)),
						this, SLOT(close()), Qt::QueuedConnection);
				controller_->disconnectHost();
			}
			event->ignore();
			return;
		}
#endif
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
#if 0
				disconnect(controller_, SIGNAL(disconnected(QString)),
						this, SLOT(close()));
#endif
				event->ignore();
				return;
			}
		}
	}
	exit();
}

/**
 * Mark window as modified
 */
void MainWindow::boardChanged()
{
	setWindowModified(true);
}

/**
 * Show the "new document" dialog
 */
void MainWindow::showNew()
{
	const QSize size = _canvas->sceneRect().size().toSize();
	if (_canvas->hasImage())
	{
		newdlg_->setNewWidth(size.width());
		newdlg_->setNewHeight(size.height());
	}
	else
	{
		newdlg_->setNewWidth(800);
		newdlg_->setNewHeight(600);
	}
	newdlg_->setNewBackground(fgbgcolor_->background());
	newdlg_->show();
}

/**
 * Initialize the board and set background color to the one
 * chosen in the dialog.
 * If the document is unsaved, create a new window.
 */
void MainWindow::newDocument()
{
	loadDocument(BlankCanvasLoader(
		QSize(newdlg_->newWidth(), newdlg_->newHeight()),
		newdlg_->newBackground()
	));
#if 0
	win->fgbgcolor_->setBackground(newdlg_->newBackground());
#endif
}

/**
 * @param action
 */
void MainWindow::openRecent(QAction *action)
{
	action->setProperty("deletelater",true);
	open(action->property("filepath").toString());
}

/**
 * Open the selected file
 * @param file file to open
 * @pre file.isEmpty()!=false
 */
void MainWindow::open(const QString& file)
{
	if(loadDocument(ImageCanvasLoader(file))) {
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
	QString formats = "*.ora ";
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	const QString filter = tr("Images (%1);;All files (*)").arg(formats);

	// Get the file name to open
	const QString file = QFileDialog::getOpenFileName(this,
			tr("Open image"), lastpath_, filter);

	// Open the file if it was selected
	if(file.isEmpty()==false) {
		const QFileInfo info(file);
		lastpath_ = info.absolutePath();

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
	if(filename_.isEmpty()) {
		return saveas();
	} else {
		if(QFileInfo(filename_).suffix() != "ora" && _canvas->needSaveOra()) {
			if(confirmFlatten(filename_)==false)
				return false;
		}
		if(_canvas->save(filename_) == false) {
			showErrorMessage(ERR_SAVE);
			return false;
		} else {
			setWindowModified(false);
			addRecentFile(filename_);
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
	// Get a list of supported formats
	foreach(QByteArray format, QImageWriter::supportedImageFormats()) {
		filter += QString(format).toUpper() + " (*." + format + ");;";
	}
#endif
	// We build the filter manually, because these are pretty much the only
	// reasonable formats (who would want to save a 1600x1200 image
	// as an XPM?). Perhaps we should check GIF support was compiled in?
	filter = "OpenRaster (*.ora);;PNG (*.png);;JPEG (*.jpeg);;BMP (*.bmp);;";
	filter += tr("All files (*)");

	// Get the file name
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save image"), lastpath_, filter, &selfilter);
	if(file.isEmpty()==false) {
		// If no file suffix is given, use a default one
		const QFileInfo info(file);
		if(info.suffix().isEmpty()) {
			// Pick the default suffix based on the features used
			if(_canvas->needSaveOra())
				file += ".ora";
			else
				file += ".png";
		} else if(_canvas->needSaveOra() && info.suffix() != "ora") {
			// If the user has already chosen a format and it lacks
			// the necessary features, confirm this is what they
			// really want to do.
			if(confirmFlatten(file)==false)
				return false;
		}

		// Save the image
		if(_canvas->save(file) == false) {
			showErrorMessage(ERR_SAVE);
			return false;
		} else {
			filename_ = file;
			setWindowModified(false);
			setTitle();
			return true;
		}
	}
	return false;
}

/**
 * The settings window will be window modal and automatically destruct
 * when it is closed.
 */
void MainWindow::showSettings()
{
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog(customacts_, this);
	connect(dlg, SIGNAL(shortcutsChanged()), this, SLOT(updateShortcuts()));
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->show();
}

void MainWindow::host()
{
	// TODO
	hostdlg_ = new dialogs::HostDialog(_canvas->image(), this);
	connect(hostdlg_, SIGNAL(finished(int)), this, SLOT(finishHost(int)));
	hostdlg_->show();
}

/**
 * Show the join dialog
 */
void MainWindow::join()
{
	joindlg_ = new dialogs::JoinDialog(this);
	connect(joindlg_, SIGNAL(finished(int)), this, SLOT(finishJoin(int)));
	joindlg_->show();
}

/**
 * Leave action triggered, ask for confirmation
 */
void MainWindow::leave()
{
	QMessageBox *leavebox = new QMessageBox(
		QMessageBox::Question,
		sessiontitle_.isEmpty()?tr("Untitled session"):sessiontitle_,
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
	connect(leavebox, SIGNAL(finished(int)), this, SLOT(finishLeave(int)));
	
#if 0
	if(controller_->isUploading()) {
		leavebox->setIcon(QMessageBox::Warning);
		leavebox->setInformativeText(tr("You are currently sending board contents to a new user. Please wait until it has been fully sent."));
	}
#endif
	leavebox->show();
}

/**
 * Leave action confirmed, disconnected.
 * 
 * @todo do this more gracefully by first sending out an unsubscribe message.
 */
void MainWindow::finishLeave(int i)
{
#if 0
	if(i == 0)
		controller_->disconnectHost();
#endif
}

/**
 * User has finally decided to connect to a host (possibly localhost)
 * and host a session.
 * @param i dialog return value
 */
void MainWindow::finishHost(int i)
{
	if(i==QDialog::Accepted) {
		const bool useremote = hostdlg_->useRemoteAddress();
		QUrl address;

		if(useremote) {
			QString scheme;
			if(hostdlg_->getRemoteAddress().startsWith("drawpile://")==false)
				scheme = "drawpile://";
			address = QUrl(scheme + hostdlg_->getRemoteAddress(),
					QUrl::TolerantMode);
		} else {
			QSettings& cfg = DrawPileApp::getSettings();
			address.setHost("127.0.0.1");
			if(cfg.contains("settings/server/port"))
				address.setPort(cfg.value("settings/server/port").toInt());
		}

		if(address.isValid() == false || address.host().isEmpty()) {
			hostdlg_->show();
			showErrorMessage(BAD_URL);
			return;
		}
		address.setUserName(hostdlg_->getUserName());

		// Remember some settings
		hostdlg_->rememberSettings();

		// Start server if hosting locally
		if(useremote==false) {
			LocalServer::startServer();
		}

		// If another image was selected, open a new window (unless this window
		// is replaceable)
#if 0 // TODO
		MainWindow *w = this;
		if(hostdlg_->useOriginalImage() == false) {
			if(!canReplace())
				w = new MainWindow(this);
			w->initBoard(hostdlg_->getImage());
		}
		w->hostSession(address, hostdlg_->getPassword(), hostdlg_->getTitle(),
				hostdlg_->getUserLimit(), hostdlg_->getAllowDrawing());
#endif

	}
	hostdlg_->deleteLater();
}

void MainWindow::hostSession(const QUrl& url, const QString& password,
		const QString& title, int userlimit, bool allowdrawing)
{
#if 0
	// Connect to host
	disconnect(controller_, SIGNAL(loggedin()), this, 0);
	controller_->hostSession(url, password,
			title, _board->image(), userlimit, allowdrawing);

	// Set login dialog to correct state
	logindlg_->connecting(url.host(), true);
	connect(logindlg_, SIGNAL(rejected()), controller_, SLOT(disconnectHost()));
#endif
}

/**
 * User has finally decided to connect to a server and join a session.
 * If there are multiple sessions, we will have to ask the user which
 * one to join later.
 */
void MainWindow::finishJoin(int i) {
	if(i==QDialog::Accepted) {
		QString scheme;
		if(joindlg_->getAddress().startsWith("drawpile://")==false)
			scheme = "drawpile://";
		QUrl address = QUrl(scheme + joindlg_->getAddress(),QUrl::TolerantMode);
		if(address.isValid()==false || address.host().isEmpty()) {
			joindlg_->show();
			showErrorMessage(BAD_URL);
			return;
		}
		address.setUserName(joindlg_->getUserName());

		// Remember some settings
		joindlg_->rememberSettings();

		// Connect
		MainWindow *win;
		if(canReplace()) {
			win = this;
		} else {
			win = new MainWindow(this);
		}
		win->joinSession(address);
	}
	joindlg_->deleteLater();
}

/**
 * Connection established, so disable and enable some UI elements
 */
void MainWindow::connected()
{
	host_->setEnabled(false);
	logout_->setEnabled(true);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::disconnected()
{
	userlist_->setSession(0);
	host_->setEnabled(true);
	logout_->setEnabled(false);
	adminTools_->setEnabled(false);
	setSessionTitle(QString());
}

/**
 * @param session the session that was joined
 */
void MainWindow::joined()
{
#if 0
	userlist_->setSession(controller_->session());
#endif
	chatbox_->joined();
}

/**
 * Board info has changed.
 * Things that can change:
 * - board size (not handled ATM)
 * - title
 * - owner
 * - board lock (handled elsewhere)
 */
void MainWindow::boardInfoChanged()
{
#if 0
	const network::SessionState *session = controller_->session();
	const network::Session& info = session->info();
	setSessionTitle(info.title());
	const bool isowner = info.owner() == session->host()->localUser();
	userlist_->setAdminMode(isowner);
	adminTools_->setEnabled(isowner);
	disallowjoins_->setChecked(info.maxUsers() <= session->userCount());
#endif
}

/**
 * Board was locked, inform the user about it.
 * @param reason the reason the board was locked
 */
void MainWindow::lock(const QString& reason)
{
	lock_board->setChecked(true);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::On));
	lockstatus_->setToolTip(tr("Board is locked"));
}

/**
 * Board is no longer locked, inform the user about it.
 */
void MainWindow::unlock()
{
	lock_board->setChecked(false);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
	lockstatus_->setToolTip(tr("Board is not locked"));
}

void MainWindow::rasterUp(int p)
{
	statusBar()->showMessage(tr("Sending board contents to new user, %1% done").arg(QString::number(p)),1000);
}

void MainWindow::setForegroundColor()
{
	fgdialog_->setColor(fgbgcolor_->foreground());
	fgdialog_->show();
}

void MainWindow::setBackgroundColor()
{
	bgdialog_->setColor(fgbgcolor_->background());
	bgdialog_->show();
}

/**
 * Session title changed
 * @param title new title
 */
void MainWindow::setSessionTitle(const QString& title)
{
	sessiontitle_ = title;
	setTitle();
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
 * @param type error type
 */
void MainWindow::showErrorMessage(ErrorType type)
{
	QString msg;
	switch(type) {
		case ERR_SAVE: msg = tr("An error occured while trying to save the image."); break;
		case ERR_OPEN: msg = tr("An error occured while trying to open the image."); break;
		case BAD_URL: msg = tr("Invalid address entered."); break;
		default: qFatal("no such error type");
	}
	showErrorMessage(msg);
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

/**
 * Increase zoom factor
 */
void MainWindow::zoomin()
{
	int nz = _view->zoom() * 2;
	if(nz>25) {
		// When zoom% is over 25, make sure we increase in nice evenly
		// dividing increments.
		if(nz % 25) nz = nz / 25 * 25;
	}

	_view->setZoom(nz);
}

/**
 * Decrease zoom factor
 */
void MainWindow::zoomout()
{
	_view->setZoom(_view->zoom() / 2);
}

/**
 * Set zoom factor to 100%
 */
void MainWindow::zoomone()
{
	_view->setZoom(100);
}

/**
 * Set rotation angle to 0
 */
void MainWindow::rotatezero()
{
	_view->setRotation(0.0);
}

void MainWindow::toggleAnnotations(bool hidden)
{
	annotationtool_->setEnabled(!hidden);
	_canvas->showAnnotations(!hidden);
	if(hidden) {
		if(annotationtool_->isChecked())
			brushtool_->trigger();
		// lasttool_ might be erasertool_ when tablet is brought near
		if(lasttool_ == annotationtool_)
			lasttool_ = brushtool_;
	}

}

/**
 * Toggle fullscreen mode for editor view
 */
void MainWindow::fullscreen(bool enable)
{
	static QByteArray oldstate;
	static QPoint oldpos;
	static QSize oldsize;
	if(enable) {
		Q_ASSERT(windowState().testFlag(Qt::WindowFullScreen)==false);
		// Save state
		oldstate = saveState();
		oldpos = pos();
		oldsize = size();
		// Hide everything except the central widget
		/** @todo hiding the menu bar disables shortcut keys */
		statusBar()->hide();
		const QObjectList c = children();
		foreach(QObject *child, c) {
			if(child->inherits("QToolBar") || child->inherits("QDockWidget"))
				(qobject_cast<QWidget*>(child))->hide();
		}
		showFullScreen();
	} else {
		Q_ASSERT(windowState().testFlag(Qt::WindowFullScreen)==true);
		// Restore old state
		showNormal();
		statusBar()->show();
		resize(oldsize);
		move(oldpos);
		restoreState(oldstate);
	}
}

/**
 * User selected a tool
 * @param tool action representing the tool
 */
void MainWindow::selectTool(QAction *tool)
{
	tools::Type type;
	if(tool == pentool_) 
		type = tools::PEN;
	else if(tool == brushtool_) 
		type = tools::BRUSH;
	else if(tool == erasertool_) 
		type = tools::ERASER;
	else if(tool == pickertool_) 
		type = tools::PICKER;
	else if(tool == linetool_) 
		type = tools::LINE;
	else if(tool == recttool_) 
		type = tools::RECTANGLE;
	else if(tool == annotationtool_)
		type = tools::ANNOTATION;
	else
		return;
	lasttool_ = tool;
	// When using the annotation tool, highlight all text boxes
	_canvas->highlightAnnotations(type==tools::ANNOTATION);
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
		QAction *lt = lasttool_; // Save lasttool_
		erasertool_->trigger();
		lasttool_ = lt;
	} else {
		lasttool_->trigger();
	}
}

void MainWindow::about()
{
	QMessageBox::about(this, tr("About DrawPile"),
			tr("<p><b>DrawPile %1</b><br>"
			"A collaborative drawing program.</p>"
			"<p>This program is free software; you may redistribute it and/or "
			"modify it under the terms of the GNU General Public License as " 
			"published by the Free Software Foundation, either version 2, or "
			"(at your opinion) any later version.</p>"
			"<p>Programming: Calle Laakkonen, M.K.A<br>"
			"Icons are from the Tango Desktop Project</p>").arg(version::string)
			);
}

void MainWindow::homepage()
{
	QDesktopServices::openUrl(QUrl("http://drawpile.sourceforge.net/"));
}

/**
 * A utility function for creating an editable action. All created actions
 * are added to a list that is used in the settings dialog to edit
 * the shortcuts.
 * @param name (internal) name of the action. If null, no name is set. If no name is set, the shortcut cannot be customized.
 * @param icon name of the icon file to use. If 0, no icon is set.
 * @param text action text
 * @param tip status bar tip
 * @param shortcut default shortcut
 */
QAction *MainWindow::makeAction(const char *name, const char *icon, const QString& text, const QString& tip, const QKeySequence& shortcut)
{
	QAction *act;
	QIcon qicon;
	if(icon)
		qicon = QIcon(QString(":icons/") + icon);
	act = new QAction(qicon, text, this);
	if(name)
		act->setObjectName(name);
	if(shortcut.isEmpty()==false) {
		act->setShortcut(shortcut);
		act->setProperty("defaultshortcut", shortcut);
	}
	if(tip.isEmpty()==false)
		act->setStatusTip(tip);

	if(name!=0 && name[0]!='\0')
		customacts_.append(act);
	return act;
}

void MainWindow::initActions()
{
	// File actions
	new_ = makeAction("newdocument", "document-new.png", tr("&New"), tr("Start a new drawing"), QKeySequence::New);
	open_ = makeAction("opendocument", "document-open.png", tr("&Open..."), tr("Open an existing drawing"), QKeySequence::Open);
	save_ = makeAction("savedocument", "document-save.png",tr("&Save"),tr("Save drawing to file"),QKeySequence::Save);
	saveas_ = makeAction("savedocumentas", "document-save-as.png", tr("Save &As..."), tr("Save drawing to a file with a new name"));
	quit_ = makeAction("exitprogram", "system-log-out.png", tr("&Quit"), tr("Quit the program"), QKeySequence("Ctrl+Q"));
	quit_->setMenuRole(QAction::QuitRole);

	// The saving actions are initially disabled, as we have no image
	save_->setEnabled(false);
	saveas_->setEnabled(false);

	connect(new_,SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open_,SIGNAL(triggered()), this, SLOT(open()));
	connect(save_,SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas_,SIGNAL(triggered()), this, SLOT(saveas()));
	connect(quit_,SIGNAL(triggered()), this, SLOT(close()));

	// Session actions
	host_ = makeAction("hostsession", 0, tr("&Host..."),tr("Share your drawingboard with others"));
	join_ = makeAction("joinsession", 0, tr("&Join..."),tr("Join another user's drawing session"));
	logout_ = makeAction("leavesession", 0, tr("&Leave"),tr("Leave this drawing session"));
	lock_board = makeAction("locksession", 0, tr("Lo&ck the board"), tr("Prevent changes to the drawing board"));
	lock_board->setCheckable(true);
	disallowjoins_ = makeAction("denyjoins", 0, tr("&Deny joins"), tr("Prevent new users from joining the session"));
	disallowjoins_->setCheckable(true);

	logout_->setEnabled(false);

	adminTools_ = new QActionGroup(this);
	adminTools_->setExclusive(false);
	adminTools_->addAction(lock_board);
	adminTools_->addAction(disallowjoins_);
	adminTools_->setEnabled(false);

	connect(host_, SIGNAL(triggered()), this, SLOT(host()));
	connect(join_, SIGNAL(triggered()), this, SLOT(join()));
	connect(logout_, SIGNAL(triggered()), this, SLOT(leave()));

	// Drawing tool actions
	pentool_ = makeAction("toolpen", "draw-freehand.png", tr("&Pen"), tr("Draw with hard strokes"), QKeySequence("P"));
	pentool_->setCheckable(true);

	brushtool_ = makeAction("toolbrush", "draw-brush.png", tr("&Brush"), tr("Draw with smooth strokes"), QKeySequence("B"));
	brushtool_->setCheckable(true); brushtool_->setChecked(true);

	erasertool_ = makeAction("tooleraser", "draw-eraser.png", tr("&Eraser"), tr("Draw with the background color"), QKeySequence("E"));
	erasertool_->setCheckable(true);

	pickertool_ = makeAction("toolpicker", "color-picker.png", tr("&Color picker"), tr("Pick colors from the image"), QKeySequence("I"));
	pickertool_->setCheckable(true);

	linetool_ = makeAction("toolline", "todo-line.png", tr("&Line"), tr("Draw straight lines"), QKeySequence("U"));
	linetool_->setCheckable(true);

	recttool_ = makeAction("toolrect", "draw-rectangle.png", tr("&Rectangle"), tr("Draw unfilled rectangles"), QKeySequence("R"));
	recttool_->setCheckable(true);

	annotationtool_ = makeAction("tooltext", "draw-text.png", tr("&Annotation"), tr("Add annotations to the picture"), QKeySequence("A"));
	annotationtool_->setCheckable(true);

	// A default
	lasttool_ = brushtool_;

	drawingtools_ = new QActionGroup(this);
	drawingtools_->setExclusive(true);
	drawingtools_->addAction(pentool_);
	drawingtools_->addAction(brushtool_);
	drawingtools_->addAction(erasertool_);
	drawingtools_->addAction(pickertool_);
	drawingtools_->addAction(linetool_);
	drawingtools_->addAction(recttool_);
	drawingtools_->addAction(annotationtool_);
	connect(drawingtools_, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	// View actions
	zoomin_ = makeAction("zoomin", "zoom-in.png",tr("Zoom &in"), QString(), QKeySequence::ZoomIn);
	zoomout_ = makeAction("zoomout", "zoom-out.png",tr("Zoom &out"), QString(), QKeySequence::ZoomOut);
	zoomorig_ = makeAction("zoomone", "zoom-original.png",tr("&Normal size"), QString(), QKeySequence(Qt::CTRL + Qt::Key_0));
	rotateorig_ = makeAction("rotatezero", "view-refresh.png",tr("&Reset rotation"), tr("Drag the view while holding ctrl-space to rotate"), QKeySequence(Qt::CTRL + Qt::Key_R));

	fullscreen_ = makeAction("fullscreen", 0, tr("&Full screen"), QString(), QKeySequence("F11"));
	fullscreen_->setCheckable(true);

	hideannotations_ = makeAction("toggleannotations", 0, tr("Hide &annotations"), QString());
	hideannotations_->setCheckable(true);

	connect(zoomin_, SIGNAL(triggered()), this, SLOT(zoomin()));
	connect(zoomout_, SIGNAL(triggered()), this, SLOT(zoomout()));
	connect(zoomorig_, SIGNAL(triggered()), this, SLOT(zoomone()));
	connect(rotateorig_, SIGNAL(triggered()), this, SLOT(rotatezero()));
	connect(fullscreen_, SIGNAL(triggered(bool)), this, SLOT(fullscreen(bool)));
	connect(hideannotations_, SIGNAL(triggered(bool)), this, SLOT(toggleAnnotations(bool)));

	// Tool cursor settings
	toggleoutline_ = makeAction("brushoutline", 0, tr("Show brush &outline"), tr("Display the brush outline around the cursor"));
	toggleoutline_->setCheckable(true);

	swapcolors_ = makeAction("swapcolors", 0, tr("Swap colors"), tr("Swap foreground and background colors"), QKeySequence(Qt::Key_X));

	// Settings window action
	settings_ = makeAction(0, 0, tr("&Settings"));
	connect(settings_, SIGNAL(triggered()), this, SLOT(showSettings()));

	// Toolbar toggling actions
	toolbartoggles_ = new QAction(tr("&Toolbars"), this);
	docktoggles_ = new QAction(tr("&Docks"), this);

	// Help actions
	homepage_ = makeAction("dphomepage", 0, tr("&DrawPile homepage"), tr("Open DrawPile homepage with the default web browser"));
	connect(homepage_,SIGNAL(triggered()), this, SLOT(homepage()));
	about_ = makeAction("dpabout", 0, tr("&About DrawPile"), tr("Show information about DrawPile"));
	about_->setMenuRole(QAction::AboutRole);
	connect(about_,SIGNAL(triggered()), this, SLOT(about()));
}

void MainWindow::createMenus()
{
	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(new_);
	filemenu->addAction(open_);
	recent_ = filemenu->addMenu(tr("Open recent"));
	filemenu->addAction(save_);
	filemenu->addAction(saveas_);
	filemenu->addSeparator();
	filemenu->addAction(quit_);

	connect(recent_, SIGNAL(triggered(QAction*)),
			this, SLOT(openRecent(QAction*)));

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(toolbartoggles_);
	viewmenu->addAction(docktoggles_);
	viewmenu->addSeparator();
	viewmenu->addAction(zoomin_);
	viewmenu->addAction(zoomout_);
	viewmenu->addAction(zoomorig_);
	viewmenu->addAction(rotateorig_);
	viewmenu->addAction(fullscreen_);
	viewmenu->addAction(hideannotations_);

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host_);
	sessionmenu->addAction(join_);
	sessionmenu->addAction(logout_);
	sessionmenu->addSeparator();
	sessionmenu->addAction(lock_board);
	sessionmenu->addAction(disallowjoins_);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addAction(pentool_);
	toolsmenu->addAction(brushtool_);
	toolsmenu->addAction(erasertool_);
	toolsmenu->addAction(pickertool_);
	toolsmenu->addAction(linetool_);
	toolsmenu->addAction(recttool_);
	toolsmenu->addAction(annotationtool_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(toggleoutline_);
	toolsmenu->addAction(swapcolors_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(settings_);

	//QMenu *settingsmenu = menuBar()->addMenu(tr("Settings"));

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage_);
	helpmenu->addSeparator();
	helpmenu->addAction(about_);
}

void MainWindow::createToolbars()
{
	QMenu *togglemenu = new QMenu(this);
	// File toolbar
	QToolBar *filetools = new QToolBar(tr("File tools"));
	filetools->setObjectName("filetoolsbar");
	togglemenu->addAction(filetools->toggleViewAction());
	filetools->addAction(new_);
	filetools->addAction(open_);
	filetools->addAction(save_);
	filetools->addAction(saveas_);
	addToolBar(Qt::TopToolBarArea, filetools);

	// Drawing toolbar
	QToolBar *drawtools = new QToolBar("Drawing tools");
	drawtools->setObjectName("drawtoolsbar");
	togglemenu->addAction(drawtools->toggleViewAction());

	drawtools->addAction(pentool_);
	drawtools->addAction(brushtool_);
	drawtools->addAction(erasertool_);
	drawtools->addAction(pickertool_);
	drawtools->addAction(linetool_);
	drawtools->addAction(recttool_);
	drawtools->addAction(annotationtool_);
	drawtools->addSeparator();
	drawtools->addAction(zoomin_);
	drawtools->addAction(zoomout_);
	drawtools->addAction(zoomorig_);
	drawtools->addAction(rotateorig_);
	drawtools->addSeparator();

	// Create color button
	fgbgcolor_ = new widgets::DualColorButton(drawtools);

	connect(swapcolors_, SIGNAL(triggered()),
			fgbgcolor_, SLOT(swapColors()));

	connect(fgbgcolor_,SIGNAL(foregroundClicked()),
			this, SLOT(setForegroundColor()));

	connect(fgbgcolor_,SIGNAL(backgroundClicked()),
			this, SLOT(setBackgroundColor()));

	// Create color changer dialogs
	fgdialog_ = new dialogs::ColorDialog(tr("Foreground color"), true, false, this);
	connect(fgdialog_, SIGNAL(colorSelected(QColor)),
			fgbgcolor_, SLOT(setForeground(QColor)));

	bgdialog_ = new dialogs::ColorDialog(tr("Background color"), true, false, this);
	connect(bgdialog_, SIGNAL(colorSelected(QColor)),
			fgbgcolor_, SLOT(setBackground(QColor)));

	drawtools->addWidget(fgbgcolor_);

	addToolBar(Qt::TopToolBarArea, drawtools);

	toolbartoggles_->setMenu(togglemenu);
}

void MainWindow::createDocks()
{
	QMenu *toggles = new QMenu(this);
	createToolSettings(toggles);
	createColorBoxes(toggles);
	createPalette(toggles);
	createUserList(toggles);
	createLayerList(toggles);
	createNavigator(toggles);
	tabifyDockWidget(hsv_, rgb_);
	tabifyDockWidget(hsv_, palette_);
	tabifyDockWidget(userlist_, _layerlist);
	docktoggles_->setMenu(toggles);
}

void MainWindow::createNavigator(QMenu *toggles)
{
	navigator_ = new widgets::Navigator(this, _canvas);
	navigator_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
	toggles->addAction(navigator_->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, navigator_);
}

void MainWindow::createToolSettings(QMenu *toggles)
{
	toolsettings_ = new widgets::ToolSettings(this);
	toolsettings_->setObjectName("toolsettingsdock");
	toolsettings_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	connect(this, SIGNAL(toolChanged(tools::Type)), toolsettings_, SLOT(setTool(tools::Type)));
	toggles->addAction(toolsettings_->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, toolsettings_);
	connect(fgbgcolor_, SIGNAL(foregroundChanged(const QColor&)), toolsettings_, SLOT(setForeground(const QColor&)));
	connect(fgbgcolor_, SIGNAL(backgroundChanged(const QColor&)), toolsettings_, SLOT(setBackground(const QColor&)));
}

void MainWindow::createUserList(QMenu *toggles)
{
	userlist_ = new widgets::UserList(this);
	userlist_->setObjectName("userlistdock");
	userlist_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	toggles->addAction(userlist_->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, userlist_);
}

void MainWindow::createLayerList(QMenu *toggles)
{
	_layerlist = new widgets::LayerListWidget(this);
	_layerlist->setObjectName("layerlistdock");
	_layerlist->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	toggles->addAction(_layerlist->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, _layerlist);
}

void MainWindow::createPalette(QMenu *toggles)
{
	palette_ = new widgets::PaletteBox(tr("Palette"), this);
	palette_->setObjectName("palettedock");
	toggles->addAction(palette_->toggleViewAction());

	connect(palette_, SIGNAL(colorSelected(QColor)),
			fgbgcolor_, SLOT(setForeground(QColor)));

	addDockWidget(Qt::RightDockWidgetArea, palette_);
}

void MainWindow::createColorBoxes(QMenu *toggles)
{
	rgb_ = new widgets::ColorBox("RGB", widgets::ColorBox::RGB, this);
	rgb_->setObjectName("rgbdock");
	toggles->addAction(rgb_->toggleViewAction());

	hsv_ = new widgets::ColorBox("HSV", widgets::ColorBox::HSV, this);
	hsv_->setObjectName("hsvdock");
	toggles->addAction(hsv_->toggleViewAction());

	connect(fgbgcolor_,SIGNAL(foregroundChanged(QColor)),
			rgb_, SLOT(setColor(QColor)));
	connect(fgbgcolor_,SIGNAL(foregroundChanged(QColor)),
			hsv_, SLOT(setColor(QColor)));

	connect(rgb_, SIGNAL(colorChanged(QColor)),
			fgbgcolor_, SLOT(setForeground(QColor)));
	connect(hsv_, SIGNAL(colorChanged(QColor)),
			fgbgcolor_, SLOT(setForeground(QColor)));

	addDockWidget(Qt::RightDockWidgetArea, rgb_);
	addDockWidget(Qt::RightDockWidgetArea, hsv_);
}


void MainWindow::createDialogs()
{
	newdlg_ = new dialogs::NewDialog(this);
	connect(newdlg_, SIGNAL(accepted()), this, SLOT(newDocument()));
}


