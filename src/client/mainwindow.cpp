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
#include <QInputDialog>
#include <QCloseEvent>
#include <QPushButton>
#include <QImageReader>
#include <QSplitter>
#include <QClipboard>

#include "config.h"
#include "main.h"
#include "mainwindow.h"
#include "loader.h"

#include "canvasview.h"
#include "canvasscene.h"
#include "annotationitem.h"
#include "selectionitem.h"
#include "statetracker.h"
#include "toolsettings.h" // for setting annotation editor widgets Client pointer

#include "utils/recentfiles.h"
#include "utils/icons.h"
#include "utils/whatismyip.h"

#include "widgets/viewstatus.h"
#include "widgets/netstatus.h"
#include "widgets/dualcolorbutton.h"
#include "widgets/chatwidget.h"

#include "docks/toolsettingswidget.h"
#include "docks/palettebox.h"
#include "docks/navigator.h"
#include "docks/colorbox.h"
#include "docks/userlistdock.h"
#include "docks/layerlistdock.h"

#include "net/client.h"
#include "net/login.h"
#include "net/serverthread.h"

#include "dialogs/colordialog.h"
#include "dialogs/newdialog.h"
#include "dialogs/hostdialog.h"
#include "dialogs/joindialog.h"
#include "dialogs/settingsdialog.h"

MainWindow::MainWindow(bool restoreWindowPosition)
	: QMainWindow(), _canvas(0)
{
	updateTitle();

	createDocks();

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	// Create the view status widget
	widgets::ViewStatus *viewstatus = new widgets::ViewStatus(this);
	statusbar->addPermanentWidget(viewstatus);

	// Create net status widget
	widgets::NetStatus *netstatus = new widgets::NetStatus(this);
	statusbar->addPermanentWidget(netstatus);

	// Create lock status widget
	_lockstatus = new QLabel(this);
	_lockstatus->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
	_lockstatus->setToolTip(tr("Board is not locked"));
	statusbar->addPermanentWidget(_lockstatus);

	// Work area is split between the canvas view and the chatbox
	splitter_ = new QSplitter(Qt::Vertical, this);
	setCentralWidget(splitter_);

	// Create canvas view
	_view = new widgets::CanvasView(this);
	_view->setToolSettings(_toolsettings);
	
	connect(_layerlist, SIGNAL(layerSelected(int)), _view, SLOT(selectLayer(int)));
	connect(_layerlist, SIGNAL(layerSelected(int)), this, SLOT(updateLockWidget()));

	splitter_->addWidget(_view);
	splitter_->setCollapsible(0, false);

	connect(_toolsettings, SIGNAL(sizeChanged(int)), _view, SLOT(setOutlineRadius(int)));
	connect(_view, SIGNAL(imageDropped(QString)), this, SLOT(open(QString)));
	connect(_view, SIGNAL(viewTransformed(int, qreal)), viewstatus, SLOT(setTransformation(int, qreal)));

	connect(this, SIGNAL(toolChanged(tools::Type)), _view, SLOT(selectTool(tools::Type)));
	
	// Create the chatbox
	widgets::ChatBox *chatbox = new widgets::ChatBox(this);
	splitter_->addWidget(chatbox);

	// Make sure the canvas gets the majority share of the splitter the first time
	splitter_->setStretchFactor(0, 1);
	splitter_->setStretchFactor(1, 0);

	// Create canvas scene
	_canvas = new drawingboard::CanvasScene(this);
	_canvas->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	_view->setCanvas(_canvas);
	navigator_->setScene(_canvas);

	connect(_canvas, SIGNAL(colorPicked(QColor)), _toolsettings->getColorPickerSettings(), SLOT(addColor(QColor)));
	connect(_canvas, &drawingboard::CanvasScene::myAnnotationCreated, _toolsettings->getAnnotationSettings(), &tools::AnnotationSettings::setSelection);
	connect(_canvas, SIGNAL(myLayerCreated(int)), _layerlist, SLOT(selectLayer(int)));
	connect(_canvas, SIGNAL(annotationDeleted(int)), _toolsettings->getAnnotationSettings(), SLOT(unselect(int)));
	connect(_canvas, &drawingboard::CanvasScene::canvasModified, [this]() { setWindowModified(true); });

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
	_toolsettings->getAnnotationSettings()->setClient(_client);
	_toolsettings->getAnnotationSettings()->setLayerSelector(_layerlist);
	_userlist->setClient(_client);

	// Client command receive signals
	connect(_client, SIGNAL(drawingCommandReceived(protocol::MessagePtr)), _canvas, SLOT(handleDrawingCommand(protocol::MessagePtr)));
	connect(_client, SIGNAL(needSnapshot(bool)), _canvas, SLOT(sendSnapshot(bool)));
	connect(_canvas, SIGNAL(newSnapshot(QList<protocol::MessagePtr>)), _client, SLOT(sendSnapshot(QList<protocol::MessagePtr>)));

	// Meta commands
	connect(_client, SIGNAL(chatMessageReceived(QString,QString, bool)),
			chatbox, SLOT(receiveMessage(QString,QString, bool)));
	connect(chatbox, SIGNAL(message(QString)), _client, SLOT(sendChat(QString)));
	connect(_client, SIGNAL(sessionTitleChange(QString)), this, SLOT(setSessionTitle(QString)));
	connect(_client, SIGNAL(opPrivilegeChange(bool)), this, SLOT(setOperatorMode(bool)));
	connect(_client, SIGNAL(sessionConfChange(bool,bool)), this, SLOT(sessionConfChanged(bool,bool)));
	connect(_client, SIGNAL(lockBitsChanged()), this, SLOT(updateLockWidget()));

	// Network status changes
	connect(_client, SIGNAL(serverConnected(QString, int)), this, SLOT(connecting()));
	connect(_client, SIGNAL(serverLoggedin(bool)), this, SLOT(loggedin(bool)));
	connect(_client, SIGNAL(serverDisconnected(QString)), this, SLOT(disconnected(QString)));

	connect(_client, SIGNAL(serverConnected(QString, int)), netstatus, SLOT(connectingToHost(QString, int)));
	connect(_client, SIGNAL(serverLoggedin(bool)), netstatus, SLOT(loggedIn()));
	connect(_client, SIGNAL(serverDisconnecting()), netstatus, SLOT(hostDisconnecting()));
	connect(_client, SIGNAL(serverDisconnected(QString)), netstatus, SLOT(hostDisconnected()));
	connect(_client, SIGNAL(expectingBytes(int)),netstatus, SLOT(expectBytes(int)));
	connect(_client, SIGNAL(bytesReceived(int)), netstatus, SLOT(bytesReceived(int)));
	connect(_client, SIGNAL(bytesSent(int)), netstatus, SLOT(bytesSent(int)));

	connect(_client, SIGNAL(userJoined(QString)), netstatus, SLOT(join(QString)));
	connect(_client, SIGNAL(userLeft(QString)), netstatus, SLOT(leave(QString)));

	connect(_client, SIGNAL(userJoined(QString)), chatbox, SLOT(userJoined(QString)));
	connect(_client, SIGNAL(userLeft(QString)), chatbox, SLOT(userParted(QString)));

	// Create actions and menus
	setupActions();

	// Restore settings
	readSettings(restoreWindowPosition);
	
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
		showErrorMessage(tr("An error occured while trying to open image"), loader.errorMessage());
		return 0;
	}

	win->_canvas->initCanvas(win->_client);
	win->_layerlist->init();
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

	win->filename_ = loader.filename();
	win->setWindowModified(false);
	win->updateTitle();
	win->_currentdoctools->setEnabled(true);
	return win;
}

/**
 * This function is used to check if the current board can be replaced
 * or if a new window is needed to open other content.
 *
 * The window can be replaced when there are no unsaved changes AND the
 * there is no active network connection
 * @retval false if a new window needs to be created
 */
bool MainWindow::canReplace() const {
	return !(isWindowModified() || _client->isConnected());
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
	if(filename_.isEmpty()) {
		name = tr("Untitled");
	} else {
		const QFileInfo info(filename_);
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
void MainWindow::readSettings(bool windowpos)
{
	QSettings& cfg = DrawPileApp::getSettings();
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
		splitter_->restoreState(cfg.value("viewstate").toByteArray());
	}

	lastpath_ = cfg.value("lastpath").toString();

	cfg.endGroup();
	cfg.beginGroup("tools");
	// Remember last used tool
	int tool = cfg.value("tool", 0).toInt();
	QList<QAction*> actions = _drawingtools->actions();
	if(tool<0 || tool>=actions.count()) tool=0;
	actions[tool]->trigger();
	_toolsettings->setTool(tools::Type(tool));

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
	dialogs::NewDialog *dlg = new dialogs::NewDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	connect(dlg, SIGNAL(accepted(QSize, QColor)), this, SLOT(newDocument(QSize, QColor)));

	if (_canvas->hasImage())
		dlg->setSize(QSize(_canvas->width(), _canvas->height()));
	else
		dlg->setSize(QSize(800, 600));

	dlg->setBackground(_fgbgcolor->background());
	dlg->show();
}

/**
 * Initialize the board and set background color to the one
 * chosen in the dialog.
 * If the document is unsaved, create a new window.
 */
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
void MainWindow::open(const QString& file)
{
	ImageCanvasLoader icl(file);
	if(loadDocument(icl)) {
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
	QString formats = "*.ora *.dptxt ";
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
		QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
		bool saved = _canvas->save(filename_);
		QApplication::restoreOverrideCursor();
		if(!saved) {
			showErrorMessage(tr("Couldn't save image"));
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
			filename_ = file;
			setWindowModified(false);
			updateTitle();
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
			address.setHost(WhatIsMyIp::localAddress());
		}

		if(address.isValid() == false || address.host().isEmpty()) {
			hostdlg_->show();
			showErrorMessage(tr("Invalid address"));
			return;
		}
		address.setUserName(hostdlg_->getUserName());

		// Remember some settings
		hostdlg_->rememberSettings();

		// Start server if hosting locally
		if(useremote==false) {
			net::ServerThread *server = new net::ServerThread(this);

			QSettings &cfg = DrawPileApp::getSettings();
			if(cfg.contains("settings/server/port"))
				server->setPort(cfg.value("settings/server/port").toInt());

			int port = server->startServer();
			if(!port) {
				QMessageBox::warning(this, tr("Unable to start server"), tr("An error occurred while trying to start the server"));
				hostdlg_->show();
				delete server;
				return;
			}
			server->setDeleteOnExit();

			if(!server->isOnDefaultPort())
				address.setPort(port);
		}

		// Initialize session (unless original was used)
		MainWindow *w = this;
		if(hostdlg_->useOriginalImage() == false) {
			QScopedPointer<SessionLoader> loader(hostdlg_->getSessionLoader());
			w = loadDocument(*loader);
		}

		// Connect to server
		net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::HOST, address);
		login->setPassword(hostdlg_->getPassword());
		login->setTitle(hostdlg_->getTitle());
		login->setMaxUsers(hostdlg_->getUserLimit());
		login->setAllowDrawing(hostdlg_->getAllowDrawing());
		w->_client->connectToServer(login);

	}
	hostdlg_->deleteLater();
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
			showErrorMessage(tr("Invalid address"));
			return;
		}
		address.setUserName(joindlg_->getUserName());

		// Remember some settings
		joindlg_->rememberSettings();

		// Connect
		joinSession(address);
	}
	joindlg_->deleteLater();
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

	net::LoginHandler *login = new net::LoginHandler(net::LoginHandler::JOIN, url);
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
void MainWindow::disconnected(const QString &message)
{
	getAction("hostsession")->setEnabled(true);
	getAction("leavesession")->setEnabled(false);
	_admintools->setEnabled(false);

	// Re-enable UI
	_view->setEnabled(true);
	_drawingtools->setEnabled(true);

	setSessionTitle(QString());

	// This should be true at this time still
	if(!_client->isLoggedIn()) {
		showErrorMessage(tr("Couldn't connect to server"), message);
	}

	// Make sure all drawing is complete
	if(_canvas->hasImage())
		_canvas->statetracker()->endRemoteContexts();
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
		_layerlist->init();
		_currentdoctools->setEnabled(true);
	}
}

void MainWindow::sessionConfChanged(bool locked, bool closed)
{
	getAction("locksession")->setChecked(locked);
	getAction("denyjoins")->setChecked(closed);
}

void MainWindow::updateLockWidget()
{
	bool locked = _client->isLocked() || _layerlist->isCurrentLayerLocked();
	if(locked) {
		_lockstatus->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::On));
		_lockstatus->setToolTip(tr("Board is locked"));
	} else {
		_lockstatus->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
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
	// Don't enable these actions in local mode
	_admintools->setEnabled(op && _client->isLoggedIn());
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

void MainWindow::copyLayer()
{
	_canvas->copyToClipboard(_layerlist->currentLayer());
}

void MainWindow::copyVisible()
{
	_canvas->copyToClipboard(0);
}

void MainWindow::paste()
{
	getAction("toolselectrect")->trigger();
	if(_canvas->hasImage()) {
		_canvas->pasteFromClipboard();
	} else {
		// Canvas not yet initialized? Initialize with clipboard content
		QImage image = QApplication::clipboard()->image();
		if(image.isNull())
			return;

		QImageCanvasLoader loader(image);
		loadDocument(loader);
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
			"Icons are from the Tango Desktop Project</p>").arg(DRAWPILE_VERSION)
			);
}

void MainWindow::homepage()
{
	QDesktopServices::openUrl(QUrl("http://drawpile.sourceforge.net/"));
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
		qicon = QIcon(QString(":icons/") + icon);
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
		customacts_.append(act);

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
	QAction *newdocument = makeAction("newdocument", "document-new.png", tr("&New"), tr("Start a new drawing"), QKeySequence::New);
	QAction *open = makeAction("opendocument", "document-open.png", tr("&Open..."), tr("Open an existing drawing"), QKeySequence::Open);
	QAction *save = makeAction("savedocument", "document-save.png",tr("&Save"),tr("Save drawing to file"),QKeySequence::Save);
	QAction *saveas = makeAction("savedocumentas", "document-save-as.png", tr("Save &As..."), tr("Save drawing to a file with a new name"));
	QAction *quit = makeAction("exitprogram", "system-log-out.png", tr("&Quit"), tr("Quit the program"), QKeySequence("Ctrl+Q"));
	quit->setMenuRole(QAction::QuitRole);

	_currentdoctools->addAction(save);
	_currentdoctools->addAction(saveas);

	connect(newdocument, SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open, SIGNAL(triggered()), this, SLOT(open()));
	connect(save, SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas, SIGNAL(triggered()), this, SLOT(saveas()));
	connect(quit, SIGNAL(triggered()), this, SLOT(close()));

	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
	filemenu->addAction(newdocument);
	filemenu->addAction(open);
	_recent = filemenu->addMenu(tr("Open recent"));
	filemenu->addAction(save);
	filemenu->addAction(saveas);
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
	QAction *undo = makeAction("undo", "edit-undo", tr("&Undo"), tr("Undo changes"), QKeySequence::Undo);
	QAction *redo = makeAction("redo", "edit-redo", tr("&Redo"), tr("Redo undo changes"), QKeySequence::Redo);
	QAction *copy = makeAction("copyvisible", "edit-copy", tr("&Copy visible"), tr("Copy selected area to the clipboard"), QKeySequence::Copy);
	QAction *copylayer = makeAction("copylayer", "edit-copy", tr("Copy &layer"), tr("Copy selected area of the current layer to the clipboard"));
	QAction *paste = makeAction("paste", "edit-paste", tr("&Paste"), tr("Paste an image onto the canvas"), QKeySequence::Paste);
	QAction *preferences = makeAction(0, 0, tr("Prefere&nces"));

	_currentdoctools->addAction(undo);
	_currentdoctools->addAction(redo);
	_currentdoctools->addAction(copy);
	_currentdoctools->addAction(copylayer);

	connect(undo, SIGNAL(triggered()), _client, SLOT(sendUndo()));
	connect(redo, SIGNAL(triggered()), _client, SLOT(sendRedo()));
	connect(copy, SIGNAL(triggered()), this, SLOT(copyVisible()));
	connect(copylayer, SIGNAL(triggered()), this, SLOT(copyLayer()));
	connect(paste, SIGNAL(triggered()), this, SLOT(paste()));
	connect(preferences, SIGNAL(triggered()), this, SLOT(showSettings()));

	QMenu *editmenu = menuBar()->addMenu(tr("&Edit"));
	editmenu->addAction(undo);
	editmenu->addAction(redo);
	editmenu->addSeparator();
	editmenu->addAction(copy);
	editmenu->addAction(copylayer);
	editmenu->addAction(paste);
	editmenu->addSeparator();
	editmenu->addAction(preferences);

	//
	// View menu
	//
	QAction *toolbartoggles = new QAction(tr("&Toolbars"), this);
	toolbartoggles->setMenu(toggletoolbarmenu);

	QAction *docktoggles = new QAction(tr("&Docks"), this);
	docktoggles->setMenu(toggledockmenu);

	QAction *zoomin = makeAction("zoomin", "zoom-in.png",tr("Zoom &in"), QString(), QKeySequence::ZoomIn);
	QAction *zoomout = makeAction("zoomout", "zoom-out.png",tr("Zoom &out"), QString(), QKeySequence::ZoomOut);
	QAction *zoomorig = makeAction("zoomone", "zoom-original.png",tr("&Normal size"), QString(), QKeySequence(Qt::CTRL + Qt::Key_0));
	QAction *rotateorig = makeAction("rotatezero", "view-refresh.png",tr("&Reset rotation"), tr("Drag the view while holding ctrl-space to rotate"), QKeySequence(Qt::CTRL + Qt::Key_R));
	QAction *rotate90 = makeAction("rotate90", 0, tr("Rotate to 90°"));
	QAction *rotate180 = makeAction("rotate180", 0, tr("Rotate to 180°"));
	QAction *rotate270 = makeAction("rotate270", 0, tr("Rotate to 270°"));

	QAction *showoutline = makeAction("brushoutline", 0, tr("Show brush &outline"), tr("Display the brush outline around the cursor"), QKeySequence(), true);
	QAction *showannotations = makeAction("showannotations", 0, tr("Show &annotations"), QString(), QKeySequence(), true);
	showannotations->setChecked(true);

	QAction *fullscreen = makeAction("fullscreen", 0, tr("&Full screen"), QString(), QKeySequence("F11"), true);

	connect(zoomin, SIGNAL(triggered()), this, SLOT(zoomin()));
	connect(zoomout, SIGNAL(triggered()), this, SLOT(zoomout()));
	connect(zoomorig, SIGNAL(triggered()), this, SLOT(zoomone()));
	connect(rotateorig, &QAction::triggered, [this]() { _view->setRotation(0); });
	connect(rotate90, &QAction::triggered, [this]() { _view->setRotation(90); });
	connect(rotate180, &QAction::triggered, [this]() { _view->setRotation(180); });
	connect(rotate270, &QAction::triggered, [this]() { _view->setRotation(270); });
	connect(fullscreen, SIGNAL(triggered(bool)), this, SLOT(fullscreen(bool)));

	connect(showoutline, SIGNAL(triggered(bool)), _view, SLOT(setOutline(bool)));
	connect(showannotations, SIGNAL(triggered(bool)), this, SLOT(setShowAnnotations(bool)));

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
	QAction *closesession = makeAction("denyjoins", 0, tr("&Deny joins"), tr("Prevent new users from joining the session"), QKeySequence(), true);

	QAction *changetitle = makeAction("changetitle", 0, tr("Change &title..."), tr("Change the session title"));

	_admintools->addAction(locksession);
	_admintools->addAction(closesession);
	_admintools->addAction(changetitle);
	_admintools->setEnabled(false);

	connect(host, SIGNAL(triggered()), this, SLOT(host()));
	connect(join, SIGNAL(triggered()), this, SLOT(join()));
	connect(logout, SIGNAL(triggered()), this, SLOT(leave()));
	connect(changetitle, SIGNAL(triggered()), this, SLOT(changeSessionTitle()));
	connect(locksession, SIGNAL(triggered(bool)), _client, SLOT(sendLockSession(bool)));
	connect(closesession, SIGNAL(triggered(bool)), _client, SLOT(sendCloseSession(bool)));

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host);
	sessionmenu->addAction(join);
	sessionmenu->addAction(logout);
	sessionmenu->addSeparator();
	sessionmenu->addAction(locksession);
	sessionmenu->addAction(closesession);
	sessionmenu->addAction(changetitle);

	//
	// Tools menu and toolbar
	//
	QAction *selectiontool = makeAction("toolselectrect", "select-rectangular", tr("&Select"), tr("Select areas for copying"), QKeySequence("S"), true);
	QAction *pentool = makeAction("toolpen", "draw-freehand.png", tr("&Pen"), tr("Draw with hard strokes"), QKeySequence("P"), true);
	QAction *brushtool = makeAction("toolbrush", "draw-brush.png", tr("&Brush"), tr("Draw with smooth strokes"), QKeySequence("B"), true);
	QAction *erasertool = makeAction("tooleraser", "draw-eraser.png", tr("&Eraser"), tr("Draw with the background color"), QKeySequence("E"), true);
	QAction *pickertool = makeAction("toolpicker", "color-picker.png", tr("&Color picker"), tr("Pick colors from the image"), QKeySequence("I"), true);
	QAction *linetool = makeAction("toolline", "todo-line.png", tr("&Line"), tr("Draw straight lines"), QKeySequence("U"), true);
	QAction *recttool = makeAction("toolrect", "draw-rectangle.png", tr("&Rectangle"), tr("Draw unfilled rectangles"), QKeySequence("R"), true);
	QAction *annotationtool = makeAction("tooltext", "draw-text.png", tr("&Annotation"), tr("Add annotations to the picture"), QKeySequence("A"), true);

	QAction *swapcolors = makeAction("swapcolors", 0, tr("Swap colors"), tr("Swap foreground and background colors"), QKeySequence(Qt::Key_X));

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
	_drawingtools->addAction(annotationtool);

	connect(_drawingtools, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addActions(_drawingtools->actions());
	toolsmenu->addSeparator();
	toolsmenu->addAction(swapcolors);

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
	connect(_fgbgcolor, SIGNAL(foregroundChanged(const QColor&)), _toolsettings, SLOT(setForeground(const QColor&)));
	connect(_fgbgcolor, SIGNAL(backgroundChanged(const QColor&)), _toolsettings, SLOT(setBackground(const QColor&)));
	connect(_toolsettings->getColorPickerSettings(), SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	connect(_canvas, SIGNAL(colorPicked(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	connect(palette_, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundChanged(QColor)), rgb_, SLOT(setColor(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundChanged(QColor)), hsv_, SLOT(setColor(QColor)));

	connect(rgb_, SIGNAL(colorChanged(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(hsv_, SIGNAL(colorChanged(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));

	// Create color changer dialogs
	_fgdialog = new dialogs::ColorDialog(tr("Foreground color"), true, false, this);
	connect(_fgdialog, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setForeground(QColor)));
	connect(_fgbgcolor, SIGNAL(foregroundClicked(QColor)), _fgdialog, SLOT(pickNewColor(QColor)));

	_bgdialog = new dialogs::ColorDialog(tr("Background color"), true, false, this);
	connect(_bgdialog, SIGNAL(colorSelected(QColor)), _fgbgcolor, SLOT(setBackground(QColor)));
	connect(_fgbgcolor, SIGNAL(backgroundClicked(QColor)), _bgdialog, SLOT(pickNewColor(QColor)));

	drawtools->addWidget(_fgbgcolor);

	addToolBar(Qt::TopToolBarArea, drawtools);

	//
	// Help menu
	//
	QAction *homepage = makeAction("dphomepage", 0, tr("&DrawPile homepage"), tr("Open DrawPile homepage with the default web browser"));
	QAction *about = makeAction("dpabout", 0, tr("&About DrawPile"), tr("Show information about DrawPile"));
	QAction *aboutqt = makeAction("aboutqt", 0, tr("About &Qt"), tr("Show Qt library version"));

	connect(about, SIGNAL(triggered()), this, SLOT(about()));
	connect(aboutqt, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
	connect(homepage, SIGNAL(triggered()), this, SLOT(homepage()));

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(homepage);
	helpmenu->addSeparator();
	helpmenu->addAction(about);
	helpmenu->addAction(aboutqt);
}

void MainWindow::createDocks()
{
	// Create tool settings
	_toolsettings = new widgets::ToolSettingsDock(this);
	_toolsettings->setObjectName("toolsettingsdock");
	_toolsettings->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	connect(this, SIGNAL(toolChanged(tools::Type)), _toolsettings, SLOT(setTool(tools::Type)));
	addDockWidget(Qt::RightDockWidgetArea, _toolsettings);

	// Create color boxes
	rgb_ = new widgets::ColorBox("RGB", widgets::ColorBox::RGB, this);
	rgb_->setObjectName("rgbdock");

	hsv_ = new widgets::ColorBox("HSV", widgets::ColorBox::HSV, this);
	hsv_->setObjectName("hsvdock");

	addDockWidget(Qt::RightDockWidgetArea, rgb_);
	addDockWidget(Qt::RightDockWidgetArea, hsv_);

	// Create palette box
	palette_ = new widgets::PaletteBox(tr("Palette"), this);
	palette_->setObjectName("palettedock");
	addDockWidget(Qt::RightDockWidgetArea, palette_);

	// Create user list
	_userlist = new widgets::UserList(this);
	_userlist->setObjectName("userlistdock");
	_userlist->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _userlist);

	// Create layer list
	_layerlist = new widgets::LayerListDock(this);
	_layerlist->setObjectName("layerlistdock");
	_layerlist->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, _layerlist);

	// Create navigator
	navigator_ = new widgets::Navigator(this, _canvas);
	navigator_->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
	addDockWidget(Qt::RightDockWidgetArea, navigator_);

	// Tabify docks
	tabifyDockWidget(hsv_, rgb_);
	tabifyDockWidget(hsv_, palette_);
	tabifyDockWidget(_userlist, _layerlist);
}
