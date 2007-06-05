#include <QDebug>
/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2007 Calle Laakkonen

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

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QFileDialog>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QUrl>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPushButton>
#include <QImageReader>
#include <QImageWriter>
#include <QSplitter>

#include "mainwindow.h"
#include "netstatus.h"
#include "editorview.h"
#include "board.h"
#include "controller.h"
#include "toolsettingswidget.h"
#include "userlistwidget.h"
#include "chatwidget.h"
#include "dualcolorbutton.h"
#include "localserver.h"
#include "recentfiles.h"
#include "hoststate.h"
#include "sessionstate.h"
#include "palettebox.h"
#include "colorbox.h"
#include "icons.h"
#include "version.h"

#include "colordialog.h"
#include "newdialog.h"
#include "hostdialog.h"
#include "joindialog.h"
#include "logindialog.h"
#include "settingsdialog.h"

/**
 * @param source if not null, clone settings from this window
 */
MainWindow::MainWindow(const MainWindow *source)
	: QMainWindow()
{
	setTitle();

	initActions();
	createMenus();
	createToolbars();
	createDocks();
	createDialogs();

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	// Create net status widget
	netstatus_ = new widgets::NetStatus(this);
	statusbar->addPermanentWidget(netstatus_);
	connect(LocalServer::getInstance(), SIGNAL(serverCrashed()),
			netstatus_, SLOT(serverCrashed()));

	// Create lock status widget
	lockstatus_ = new QLabel(this);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
	lockstatus_->setToolTip(tr("Board is not locked"));
	statusbar->addPermanentWidget(lockstatus_);

	// Work area is split between the view and the chatbox
	splitter_ = new QSplitter(Qt::Vertical, this);
	setCentralWidget(splitter_);

	// Create view
	view_ = new widgets::EditorView(this);
	view_->setAcceptDrops(true);

	splitter_->addWidget(view_);
	splitter_->setCollapsible(0, false);

	connect(toolsettings_, SIGNAL(sizeChanged(int)),
			view_, SLOT(setOutlineRadius(int)));
	connect(toggleoutline_, SIGNAL(triggered(bool)),
			view_, SLOT(setOutline(bool)));
	connect(togglecrosshair_, SIGNAL(triggered(bool)),
			view_, SLOT(setCrosshair(bool)));
	connect(toolsettings_, SIGNAL(colorsChanged(const QColor&, const QColor&)),
			view_, SLOT(setOutlineColors(const QColor&, const QColor&)));
	connect(view_, SIGNAL(imageDropped(QString)),
			this, SLOT(open(QString)));


	// Create the chatbox
	chatbox_ = new widgets::ChatBox(this);
	splitter_->addWidget(chatbox_);

	// Create board
	board_ = new drawingboard::Board(this, toolsettings_, fgbgcolor_);
	board_->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	view_->setBoard(board_);

	// Create controller
	controller_ = new Controller(this);
	controller_->setModel(board_);
	connect(controller_, SIGNAL(changed()),
			this, SLOT(boardChanged()));
	connect(this, SIGNAL(toolChanged(tools::Type)),
			controller_, SLOT(setTool(tools::Type)));

	connect(view_,SIGNAL(penDown(drawingboard::Point)),
			controller_,SLOT(penDown(drawingboard::Point)));
	connect(view_,SIGNAL(penMove(drawingboard::Point)),
			controller_,SLOT(penMove(drawingboard::Point)));
	connect(view_,SIGNAL(penUp()),
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
	connect(lockboard_, SIGNAL(triggered(bool)),
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
	connect(controller_, SIGNAL(joinsDisallowed(bool)),
			disallowjoins_, SLOT(setChecked(bool)));
	connect(controller_, SIGNAL(joined(network::SessionState*)),
			this, SLOT(joined(network::SessionState*)));
	connect(controller_, SIGNAL(becameOwner()),
			this, SLOT(becameOwner()));
	connect(controller_, SIGNAL(rasterUploadProgress(int)),
			this, SLOT(rasterUp(int)));

	// Controller <-> login dialog connections
	connect(controller_, SIGNAL(connected(const QString&)),
			logindlg_, SLOT(connected()));
	connect(controller_, SIGNAL(disconnected(QString)),
			logindlg_, SLOT(disconnected(QString)));
	connect(controller_, SIGNAL(loggedin()), logindlg_,
			SLOT(loggedin()));
	connect(controller_, SIGNAL(joined(network::SessionState*)),
			logindlg_, SLOT(joined()));
	connect(controller_, SIGNAL(rasterDownloadProgress(int)),
			logindlg_, SLOT(raster(int)));
	connect(controller_, SIGNAL(noSessions()),
			logindlg_, SLOT(noSessions()));
	connect(controller_, SIGNAL(sessionNotFound()),
			logindlg_, SLOT(sessionNotFound()));
	connect(controller_, SIGNAL(netError(QString)),
			logindlg_, SLOT(error(QString)));
	connect(controller_, SIGNAL(selectSession(network::SessionList)),
			logindlg_, SLOT(selectSession(network::SessionList)));
	connect(controller_, SIGNAL(needPassword()),
			logindlg_, SLOT(getPassword()));
	connect(logindlg_, SIGNAL(session(int)),
			controller_, SLOT(joinSession(int)));
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

	if(source)
		cloneSettings(source);
	else
		readSettings();

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
 * @param filename file to open
 * @return true on success
 */
bool MainWindow::initBoard(const QString& filename)
{
	QImage image;
	if(image.load(filename)==false)
		return false;
	board_->initBoard(image);
	setWindowModified(false);
	filename_ = filename;
	setTitle();
	return true;
}

/**
 * @param size board size
 * @param color background color
 */
void MainWindow::initBoard(const QSize& size, const QColor& color)
{
	board_->initBoard(size,color);
	filename_ = "";
	setWindowModified(false);
	setTitle();
}

void MainWindow::initDefaultBoard()
{
	initBoard(QSize(800,600), Qt::white);
}

/**
 * @param image board image
 */
void MainWindow::initBoard(const QImage& image)
{
	board_->initBoard(image);
	filename_ = "";
	setWindowModified(false);
	setTitle();
}

/**
 * If a session (path) was present in the url, use it. Otherwise connect
 * normally. (automatically join if there is only one session, otherwise
 * display a list for the user)
 * @param url URL
 */
void MainWindow::joinSession(const QUrl& url)
{
	disconnect(controller_, SIGNAL(loggedin()), this, 0);

	// If no path was specified, automatically join the only available
	// session or ask the user if there are more than one.
	if(url.path().length()<=1)
		connect(controller_, SIGNAL(loggedin()), this, SLOT(loggedinJoin()));
	controller_->connectHost(url);

	// Set login dialog to correct state
	logindlg_->connecting(url.host());
	connect(logindlg_, SIGNAL(rejected()), controller_, SLOT(disconnectHost()));
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
	return isWindowModified()==false && controller_->isConnected()==false;
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
 * Read and apply mainwindow related settings.
 */
void MainWindow::readSettings()
{
	QSettings cfg;
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
	controller_->setTool(tools::Type(tool));

	// Remember cursor settings
	toggleoutline_->setChecked(cfg.value("outline",true).toBool());
	view_->setOutline(toggleoutline_->isChecked());
	togglecrosshair_->setChecked(cfg.value("crosshair",true).toBool());
	view_->setCrosshair(togglecrosshair_->isChecked());

	// Remember foreground and background colors
	const QColor fg = cfg.value("foreground", Qt::black).value<QColor>();
	const QColor bg = cfg.value("background", Qt::white).value<QColor>();
	fgbgcolor_->setForeground(fg);
	fgbgcolor_->setBackground(bg);

	// Remember recent files
	RecentFiles::initMenu(recent_);
}

/**
 * @param source window whose settings are cloned
 */
void MainWindow::cloneSettings(const MainWindow *source)
{
	// Clone size, but let the window manager position this window
	resize(source->size());

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
	controller_->setTool(tools::Type(tool));

	// Copy foreground and background colors
	fgbgcolor_->setForeground(source->fgbgcolor_->foreground());
	fgbgcolor_->setBackground(source->fgbgcolor_->background());
}


/**
 * Write out settings
 */
void MainWindow::writeSettings()
{
	QSettings cfg;
	cfg.beginGroup("window");

	if(isMaximized() == false) {
		cfg.setValue("pos", pos());
		cfg.setValue("size", size());
	}
	cfg.setValue("maximized", isMaximized());
	cfg.setValue("state", saveState());
	cfg.setValue("viewstate", splitter_->saveState());
	cfg.setValue("lastpath", lastpath_);

	cfg.endGroup();
	cfg.beginGroup("tools");
	const int tool = drawingtools_->actions().indexOf(drawingtools_->checkedAction());
	cfg.setValue("tool", tool);
	cfg.setValue("outline", toggleoutline_->isChecked());
	cfg.setValue("crosshair", togglecrosshair_->isChecked());
	cfg.setValue("foreground",fgbgcolor_->foreground());
	cfg.setValue("background",fgbgcolor_->background());
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
		if(controller_->isConnected()) {
			QMessageBox box(QMessageBox::Information, tr("DrawPile"),
					controller_->isUploading()?
					tr("You are currently sending board contents to a new user. Please wait until it has been fully sent."):
					tr("You are still connected to a drawing session."));

			const QPushButton *exitbtn = box.addButton(tr("Exit anyway"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Cancel"),
					QMessageBox::RejectRole);

			box.exec();
			if(box.clickedButton() == exitbtn) {
				// Delay exiting until actually disconnected
				connect(controller_, SIGNAL(disconnected(QString)),
						this, SLOT(close()));
				controller_->disconnectHost();
			}
			event->ignore();
			return;
		}

		// Then confirm unsaved changes
		if(isWindowModified()) {
			QMessageBox box(QMessageBox::Question, tr("DrawPile"),
					tr("There are unsaved changes. Save them before exiting?"));
			const QPushButton *savebtn = box.addButton(tr("Save changes"),
					QMessageBox::AcceptRole);
			box.addButton(tr("Exit without saving"),
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
	const QSize size = board_->sceneRect().size().toSize();
	newdlg_->setNewWidth(size.width());
	newdlg_->setNewHeight(size.height());
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
	MainWindow *win;
	if(canReplace()) {
		win = this;
	} else {
		win = new MainWindow(this);
		win->show();
	}

	win->initBoard(QSize(newdlg_->newWidth(), newdlg_->newHeight()),
			newdlg_->newBackground());
	win->fgbgcolor_->setBackground(newdlg_->newBackground());
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
	if(canReplace()) {
		if(initBoard(file)==false)
			showErrorMessage(ERR_OPEN);
		else
			addRecentFile(file);
	} else {
		MainWindow *win = new MainWindow(this);
		if(win->initBoard(file)==false) {
			showErrorMessage(ERR_OPEN);
			delete win;
		} else {
			addRecentFile(file);
			win->show();
		}
	}
}

/**
 * Show a file selector dialog. If there are unsaved changes, open the file
 * in a new window.
 */
void MainWindow::open()
{
	// Get a list of supported formats
	QString formats;
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
 * If no file name has been selected, \a saveas is called.
 */
bool MainWindow::save()
{
	if(filename_.isEmpty()) {
		return saveas();
	} else {
		if(board_->image().save(filename_) == false) {
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
 * If no suffix is entered, ".png" is used.
 */
bool MainWindow::saveas()
{
	QString selfilter;
	QString filter;
	// Get a list of supported formats
	foreach(QByteArray format, QImageWriter::supportedImageFormats()) {
		filter += QString(format).toUpper() + " (*." + format + ");;";
	}
	filter += tr("All files (*)");

	// Get the file name
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save image"), lastpath_, filter, &selfilter);
	if(file.isEmpty()==false) {
		// Get the default suffix to use
		QRegExp extexp("\\(\\*\\.(.*)\\)");
		QString defaultsuffix;
		if(extexp.indexIn(selfilter)!=-1)
			defaultsuffix = extexp.cap(1);

		// Add suffix if missing
		const QFileInfo info(file);
		lastpath_ = info.absolutePath();
		if(defaultsuffix.isEmpty()==false && info.suffix().compare(defaultsuffix)!=0)
			file += "." + defaultsuffix;

		// Save the image
		if(board_->image().save(file) == false) {
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
	dialogs::SettingsDialog *dlg = new dialogs::SettingsDialog(this);
	dlg->setAttribute(Qt::WA_DeleteOnClose);
	dlg->setWindowModality(Qt::WindowModal);
	dlg->show();
}

void MainWindow::host()
{
	hostdlg_ = new dialogs::HostDialog(board_->image(), this);
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
	leavebox_->setWindowTitle(sessiontitle_.isEmpty()?tr("Untitled session"):sessiontitle_);
	if(controller_->isUploading()) {
		leavebox_->setIcon(QMessageBox::Warning);
		leavebox_->setInformativeText(tr("You are currently sending board contents to a new user. Please wait until it has been fully sent."));
	} else {
		leavebox_->setIcon(QMessageBox::Question);
		leavebox_->setInformativeText(QString());
	}
	leavebox_->show();
}

/**
 * Leave action confirmed, disconnected.
 * 
 * TODO, do this more gracefully by first sending out an unsubscribe
 * message.
 */
void MainWindow::finishLeave(int i)
{
	if(i == 0)
		controller_->disconnectHost();
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
		QString admin;

		if(useremote) {
			QString scheme;
			if(hostdlg_->getRemoteAddress().startsWith("drawpile://")==false)
				scheme = "drawpile://";
			address = QUrl(scheme + hostdlg_->getRemoteAddress(),
					QUrl::TolerantMode);
			admin = hostdlg_->getAdminPassword();
		} else {
			QSettings cfg;
			address.setHost(LocalServer::address());
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
			LocalServer *srv = LocalServer::getInstance();
			if(srv->ensureRunning()==false) {
				showErrorMessage(srv->errorString(),srv->serverOutput());
				hostdlg_->deleteLater();
				return;
			}
		}

		// If another image was selected, replace current board with it
		// TODO, what if there were unsaved changes?
		if(hostdlg_->useOriginalImage() == false) {
			initBoard(hostdlg_->getImage());
		}

		// Connect
		disconnect(controller_, SIGNAL(loggedin()), this, 0);
		connect(controller_, SIGNAL(loggedin()), this, SLOT(loggedinHost()));
		controller_->connectHost(address, admin);

		// Set login dialog to correct state
		logindlg_->connecting(address.host());
		connect(logindlg_, SIGNAL(rejected()), controller_, SLOT(disconnectHost()));
	} else {
		hostdlg_->deleteLater();
	}
}

/**
 * User has logged in, now create a session
 */
void MainWindow::loggedinHost()
{
	controller_->hostSession(hostdlg_->getTitle(), hostdlg_->getPassword(),
			hostdlg_->getImage(), hostdlg_->getUserLimit(),
			hostdlg_->getAllowDrawing(), hostdlg_->getAllowChat());
	// Host dialog is no longer needed
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
			win->show();
		}
		win->joinSession(address);
	}
	joindlg_->deleteLater();
}

/**
 * User has logged in, now join a session
 */
void MainWindow::loggedinJoin()
{
	controller_->joinSession();
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
	host_->setEnabled(true);
	logout_->setEnabled(false);
	adminTools_->setEnabled(false);
	setSessionTitle(QString());
	userlist_->setSession(0);
}

/**
 * @param session the session that was joined
 */
void MainWindow::joined(network::SessionState *session)
{
	setSessionTitle(session->info().title);
	const bool isowner = session->info().id == session->host()->localUser().id();
	userlist_->setSession(session);
	userlist_->setAdminMode(isowner);
	adminTools_->setEnabled(isowner);
	chatbox_->joined(session->info().title, session->host()->localUser().name());
}

void MainWindow::becameOwner()
{
	adminTools_->setEnabled(true);
	userlist_->setAdminMode(true);
}

/**
 * Board was locked, inform the user about it.
 * @param reason the reason the board was locked
 */
void MainWindow::lock(const QString& reason)
{
	lockboard_->setChecked(true);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::On));
	lockstatus_->setToolTip(tr("Board is locked"));
}

/**
 * Board is no longer locked, inform the user about it.
 */
void MainWindow::unlock()
{
	lockboard_->setChecked(false);
	lockstatus_->setPixmap(icon::lock().pixmap(16,QIcon::Normal,QIcon::Off));
	lockstatus_->setToolTip(tr("Board is not locked"));
}

/**
 * Updates the allow join action checked status.
 * @param allow if true, new users may join
 */
void MainWindow::allowJoins(bool allow)
{
	disallowjoins_->setChecked(!allow);
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
	msgbox_->setStandardButtons(QMessageBox::Ok);
	msgbox_->setIcon(QMessageBox::Warning);
	msgbox_->setText(message);
	msgbox_->setDetailedText(details);
	msgbox_->show();
}

/**
 * View translation matrix is scaled
 */
void MainWindow::zoomin()
{
	view_->scale(2.0,2.0);
}

/**
 * View translation matrix is scaled
 */
void MainWindow::zoomout()
{
	view_->scale(0.5,0.5);
}

/**
 * View translation matrix is reset
 */
void MainWindow::zoomone()
{
	view_->resetMatrix();
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
		// TODO, hiding the menu bar disables shortcut keys
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
	if(tool == brushtool_) {
		type = tools::BRUSH;
	} else if(tool == erasertool_) {
		type = tools::ERASER;
	} else if(tool == pickertool_) {
		type = tools::PICKER;
	} else if(tool == linetool_) {
		type = tools::LINE;
	} else if(tool == recttool_) {
		type = tools::RECTANGLE;
	} else {
		return;
	}
	emit toolChanged(type);
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
			"<p>Programming: Calle Laakkonen<br>"
			"Graphic design: wuf<br>"
			"Server: M.K.A<br>"
			"Icons are from the Tango Desktop Project</p>").arg(version::string)
			);
}

void MainWindow::help()
{
}

void MainWindow::homepage()
{
	QDesktopServices::openUrl(QUrl("http://drawpile.sourceforge.net/"));
}

void MainWindow::initActions()
{
	// File actions
	new_ = new QAction(QIcon(":icons/document-new.png"),tr("&New"), this);
	new_->setShortcut(QKeySequence::New);
	new_->setStatusTip(tr("Start a new drawing"));
	open_ = new QAction(QIcon(":icons/document-open.png"),tr("&Open..."), this);
	open_->setShortcut(QKeySequence::Open);
	open_->setStatusTip(tr("Open an existing drawing"));
	save_ = new QAction(QIcon(":icons/document-save.png"),tr("&Save"), this);
	save_->setShortcut(QKeySequence::Save);
	save_->setStatusTip(tr("Save drawing to file"));
	saveas_ = new QAction(QIcon(":icons/document-save-as.png"),tr("Save &As..."), this);
	saveas_->setStatusTip(tr("Save drawing to file with a new name"));
	quit_ = new QAction(QIcon(":icons/system-log-out.png"),tr("&Quit"), this);
	quit_->setStatusTip(tr("Quit the program"));
	quit_->setShortcut(QKeySequence("Ctrl+Q"));
	quit_->setMenuRole(QAction::QuitRole);

	connect(new_,SIGNAL(triggered()), this, SLOT(showNew()));
	connect(open_,SIGNAL(triggered()), this, SLOT(open()));
	connect(save_,SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas_,SIGNAL(triggered()), this, SLOT(saveas()));
	connect(quit_,SIGNAL(triggered()), this, SLOT(close()));

	// Session actions
	host_ = new QAction("&Host...", this);
	host_->setStatusTip(tr("Share your drawingboard with others"));
	join_ = new QAction("&Join...", this);
	join_->setStatusTip(tr("Join another user's drawing session"));
	logout_ = new QAction("&Leave", this);
	logout_->setStatusTip(tr("Leave this drawing session"));
	lockboard_ = new QAction("Lo&ck the board", this);
	lockboard_->setStatusTip(tr("Prevent changes to the drawing board"));
	lockboard_->setCheckable(true);
	disallowjoins_ = new QAction("&Deny joins", this);
	disallowjoins_->setStatusTip(tr("Prevent new users from joining the session"));
	disallowjoins_->setCheckable(true);

	logout_->setEnabled(false);

	adminTools_ = new QActionGroup(this);
	adminTools_->setExclusive(false);
	adminTools_->addAction(lockboard_);
	adminTools_->addAction(disallowjoins_);
	adminTools_->setEnabled(false);

	connect(host_, SIGNAL(triggered()), this, SLOT(host()));
	connect(join_, SIGNAL(triggered()), this, SLOT(join()));
	connect(logout_, SIGNAL(triggered()), this, SLOT(leave()));

	// Drawing tool actions
	brushtool_ = new QAction(QIcon(":icons/draw-brush.png"),tr("&Brush"), this);
	brushtool_->setCheckable(true); brushtool_->setChecked(true);
	brushtool_->setShortcut(QKeySequence("B"));
	erasertool_ = new QAction(QIcon(":icons/draw-eraser.png"),tr("&Eraser"), this);
	erasertool_->setCheckable(true);
	erasertool_->setShortcut(QKeySequence("E"));
	pickertool_ = new QAction(QIcon(":icons/color-picker.png"),tr("&Color picker"), this);
	pickertool_->setCheckable(true);
	pickertool_->setShortcut(QKeySequence("I"));
	linetool_ = new QAction(QIcon(":icons/todo-line.png"),tr("&Line"), this);
	linetool_->setCheckable(true);
	linetool_->setShortcut(QKeySequence("U"));
	recttool_ = new QAction(QIcon(":icons/draw-rectangle.png"),tr("&Rectangle"), this);
	recttool_->setCheckable(true);
	recttool_->setShortcut(QKeySequence("R"));

	drawingtools_ = new QActionGroup(this);
	drawingtools_->setExclusive(true);
	drawingtools_->addAction(brushtool_);
	drawingtools_->addAction(erasertool_);
	drawingtools_->addAction(pickertool_);
	drawingtools_->addAction(linetool_);
	drawingtools_->addAction(recttool_);
	connect(drawingtools_, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	// View actions
	zoomin_ = new QAction(QIcon(":icons/zoom-in.png"),tr("Zoom &in"), this);
	zoomin_->setShortcut(QKeySequence::ZoomIn);
	zoomout_ = new QAction(QIcon(":icons/zoom-out.png"),tr("Zoom &out"), this);
	zoomout_->setShortcut(QKeySequence::ZoomOut);
	zoomorig_ = new QAction(QIcon(":icons/zoom-original.png"),tr("&Normal size"), this);
	//zoomorig_->setShortcut(QKeySequence::ZoomOut);
	fullscreen_ = new QAction(tr("&Full Screen"), this);
	fullscreen_->setCheckable(true);
	fullscreen_->setShortcut(QKeySequence("F11"));

	connect(zoomin_, SIGNAL(triggered()), this, SLOT(zoomin()));
	connect(zoomout_, SIGNAL(triggered()), this, SLOT(zoomout()));
	connect(zoomorig_, SIGNAL(triggered()), this, SLOT(zoomone()));
	connect(fullscreen_, SIGNAL(triggered(bool)), this, SLOT(fullscreen(bool)));

	// Tool cursor settings
	toggleoutline_ = new QAction(tr("Show brush &outline"), this);
	toggleoutline_->setStatusTip(tr("Display the brush outline around the cursor"));
	toggleoutline_->setCheckable(true);

	togglecrosshair_ = new QAction(tr("Crosshair c&ursor"), this);
	togglecrosshair_->setStatusTip(tr("Use a crosshair cursor"));
	togglecrosshair_->setCheckable(true);

	// Settings window action
	settings_ = new QAction(tr("&Settings"), this);
	connect(settings_, SIGNAL(triggered()), this, SLOT(showSettings()));

	// Toolbar toggling actions
	toolbartoggles_ = new QAction(tr("&Toolbars"), this);
	docktoggles_ = new QAction(tr("&Docks"), this);

	// Help actions
	help_ = new QAction(tr("DrawPile &Help"), this);
	help_->setShortcut(QKeySequence("F1"));
	help_->setEnabled(false);
	connect(help_,SIGNAL(triggered()), this, SLOT(help()));
	homepage_ = new QAction(tr("&DrawPile homepage"), this);
	connect(homepage_,SIGNAL(triggered()), this, SLOT(homepage()));
	about_ = new QAction(tr("&About DrawPile"), this);
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
	viewmenu->addAction(fullscreen_);

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host_);
	sessionmenu->addAction(join_);
	sessionmenu->addAction(logout_);
	sessionmenu->addSeparator();
	sessionmenu->addAction(lockboard_);
	sessionmenu->addAction(disallowjoins_);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addAction(brushtool_);
	toolsmenu->addAction(erasertool_);
	toolsmenu->addAction(pickertool_);
	toolsmenu->addAction(linetool_);
	toolsmenu->addAction(recttool_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(toggleoutline_);
	toolsmenu->addAction(togglecrosshair_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(settings_);

	//QMenu *settingsmenu = menuBar()->addMenu(tr("Settings"));

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(help_);
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

	drawtools->addAction(brushtool_);
	drawtools->addAction(erasertool_);
	drawtools->addAction(pickertool_);
	drawtools->addAction(linetool_);
	drawtools->addAction(recttool_);
	drawtools->addSeparator();
	drawtools->addAction(zoomin_);
	drawtools->addAction(zoomout_);
	drawtools->addAction(zoomorig_);
	drawtools->addSeparator();

	// Create color button
	fgbgcolor_ = new widgets::DualColorButton(drawtools);

	connect(fgbgcolor_,SIGNAL(foregroundClicked()),
			this, SLOT(setForegroundColor()));

	connect(fgbgcolor_,SIGNAL(backgroundClicked()),
			this, SLOT(setBackgroundColor()));

	// Create color changer dialogs
	fgdialog_ = new dialogs::ColorDialog(tr("Foreground color"), this);
	connect(fgdialog_, SIGNAL(colorSelected(QColor)),
			fgbgcolor_, SLOT(setForeground(QColor)));

	bgdialog_ = new dialogs::ColorDialog(tr("Background color"), this);
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
	//tabifyDockWidget(rgb_, palette_);
	tabifyDockWidget(hsv_, rgb_);
	docktoggles_->setMenu(toggles);
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

	msgbox_ = new QMessageBox(this);
	msgbox_->setWindowTitle(tr("DrawPile"));
	msgbox_->setWindowModality(Qt::WindowModal);
	msgbox_->setWindowFlags(msgbox_->windowFlags() | Qt::Sheet);

	leavebox_ = new QMessageBox(QMessageBox::Question,
			"",
			tr("Really leave the session?"),
                QMessageBox::NoButton,
                this, Qt::MSWindowsFixedSizeDialogHint|Qt::Sheet);

	leavebox_->addButton(tr("Leave"), QMessageBox::YesRole);
	leavebox_->setDefaultButton(
			leavebox_->addButton(tr("Stay"), QMessageBox::NoRole)
			);
	connect(leavebox_, SIGNAL(finished(int)), this, SLOT(finishLeave(int)));

	logindlg_ = new dialogs::LoginDialog(this);
}

