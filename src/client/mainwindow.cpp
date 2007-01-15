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
#include <QUrl>
#include <QMessageBox>
#include <QCloseEvent>
#include <QAbstractButton>
#include <QImageReader>
#include <QImageWriter>

#include "mainwindow.h"
#include "netstatus.h"
#include "editorview.h"
#include "board.h"
#include "controller.h"
#include "toolsettingswidget.h"
#include "userlistwidget.h"
#include "dualcolorbutton.h"
#include "localserver.h"

#include "aboutdialog.h"
#include "colordialog.h"
#include "newdialog.h"
#include "hostdialog.h"
#include "joindialog.h"
#include "logindialog.h"

MainWindow::MainWindow()
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

	// Create view
	view_ = new widgets::EditorView(this);
	connect(toolsettings_, SIGNAL(sizeChanged(int)), view_, SLOT(setOutlineRadius(int)));
	connect(toggleoutline_, SIGNAL(triggered(bool)), view_, SLOT(setOutline(bool)));
	connect(togglecrosshair_, SIGNAL(triggered(bool)), view_, SLOT(setCrosshair(bool)));
	connect(toolsettings_, SIGNAL(colorsChanged(const QColor&, const QColor&)), view_, SLOT(setOutlineColors(const QColor&, const QColor&)));
	setCentralWidget(view_);

	// Create board
	board_ = new drawingboard::Board(this, toolsettings_, fgbgcolor_);
	board_->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	view_->setBoard(board_);

	// Create controller
	controller_ = new Controller(this);
	controller_->setModel(board_);
	connect(controller_, SIGNAL(changed()), this, SLOT(boardChanged()));
	connect(this, SIGNAL(toolChanged(tools::Type)), controller_, SLOT(setTool(tools::Type)));

	connect(view_,SIGNAL(penDown(drawingboard::Point)),controller_,SLOT(penDown(drawingboard::Point)));
	connect(view_,SIGNAL(penMove(drawingboard::Point)),controller_,SLOT(penMove(drawingboard::Point)));
	connect(view_,SIGNAL(penUp()),controller_,SLOT(penUp()));
	// Controller -> netstatus
	connect(controller_, SIGNAL(disconnected(QString)), netstatus_, SLOT(disconnectHost()));
	connect(controller_, SIGNAL(connected(const QString&)), netstatus_, SLOT(connectHost(const QString&)));
	connect(controller_, SIGNAL(userJoined(network::User)), netstatus_, SLOT(join(network::User)));
	connect(controller_, SIGNAL(userParted(network::User)), netstatus_, SLOT(leave(network::User)));
	// User list
	connect(controller_, SIGNAL(userJoined(network::User)), userlist_, SLOT(addUser(network::User)));
	connect(controller_, SIGNAL(userParted(network::User)), userlist_, SLOT(removeUser(network::User)));
	connect(controller_, SIGNAL(disconnected(QString)), userlist_, SLOT(clearUsers()));
	connect(userlist_, SIGNAL(kick(int)), controller_, SLOT(kickUser(int)));
	connect(userlist_, SIGNAL(lock(int, bool)), controller_, SLOT(lockUser(int, bool)));
	// Controller <- actions
	connect(lockboard_, SIGNAL(triggered(bool)), controller_, SLOT(lockBoard(bool)));
	// Controller <-> mainwindow
	connect(controller_, SIGNAL(connected(QString)), this, SLOT(connected()));
	connect(controller_, SIGNAL(disconnected(QString)), this, SLOT(disconnected()));
	connect(controller_, SIGNAL(lockboard(QString)), this, SLOT(lock(QString)));
	connect(controller_, SIGNAL(unlockboard()), this, SLOT(unlock()));

	// Controller <-> login dialog connections
	connect(controller_, SIGNAL(connected(const QString&)), logindlg_, SLOT(connected()));
	connect(controller_, SIGNAL(disconnected(QString)), logindlg_, SLOT(disconnected(QString)));
	connect(controller_, SIGNAL(loggedin()), logindlg_, SLOT(loggedin()));
	connect(controller_, SIGNAL(joined(QString)), logindlg_, SLOT(joined()));
	connect(controller_, SIGNAL(joined(QString)), this, SLOT(joined(QString)));
	connect(controller_, SIGNAL(rasterDownloadProgress(int)), logindlg_, SLOT(raster(int)));
	connect(controller_, SIGNAL(rasterUploadProgress(int)),this, SLOT(rasterUp(int)));
	connect(controller_, SIGNAL(noSessions()),logindlg_, SLOT(noSessions()));
	connect(controller_, SIGNAL(sessionNotFound()),logindlg_, SLOT(sessionNotFound()));
	connect(controller_, SIGNAL(netError(QString)),logindlg_, SLOT(error(QString)));
	connect(controller_, SIGNAL(selectSession(network::SessionList)),logindlg_, SLOT(selectSession(network::SessionList)));
	connect(controller_, SIGNAL(needPassword()),logindlg_, SLOT(getPassword()));
	connect(logindlg_, SIGNAL(session(int)), controller_, SLOT(joinSession(int)));
	connect(logindlg_, SIGNAL(password(QString)), controller_, SLOT(sendPassword(QString)));

	readSettings();
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
 * @param url URL
 */
void MainWindow::joinSession(const QUrl& url)
{
	disconnect(controller_, SIGNAL(loggedin()), this, 0);

	// If a session was already specified, use that
	if(url.path().length()<=1)
		connect(controller_, SIGNAL(loggedin()), this, SLOT(loggedinJoin()));
	controller_->connectHost(url);

	// Set login dialog to correct state
	logindlg_->connecting(url.host());
	connect(logindlg_, SIGNAL(rejected()), controller_, SLOT(disconnectHost()));
}

void MainWindow::setTitle()
{
	QString name;
	if(filename_.isEmpty()) {
		name = tr("Untitled");
	} else {
		QFileInfo info(filename_);
		name = info.baseName();
	}

	if(sessiontitle_.isEmpty())
		setWindowTitle(tr("%1[*] - DrawPile").arg(name));
	else
		setWindowTitle(tr("%1[*] - %2 - DrawPile").arg(name).arg(sessiontitle_));
}

void MainWindow::readSettings()
{
	QSettings cfg;
	cfg.beginGroup("mainwindow");

	resize(cfg.value("size",QSize(800,600)).toSize());

	if(cfg.contains("pos"))
		move(cfg.value("pos").toPoint());

	if(cfg.contains("state")) {
		restoreState(cfg.value("state").toByteArray());
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
	QColor fg = cfg.value("foreground", Qt::black).value<QColor>();
	QColor bg = cfg.value("background", Qt::white).value<QColor>();
	fgbgcolor_->setForeground(fg);
	fgbgcolor_->setBackground(bg);
	fgdialog_->setColor(fg);
	bgdialog_->setColor(bg);
}

void MainWindow::writeSettings()
{
	QSettings cfg;
	cfg.beginGroup("mainwindow");

	cfg.setValue("pos", pos());
	cfg.setValue("size", size());
	cfg.setValue("state", saveState());
	cfg.setValue("lastpath", lastpath_);

	cfg.endGroup();
	cfg.beginGroup("tools");
	int tool = drawingtools_->actions().indexOf(drawingtools_->checkedAction());
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
	if(isWindowModified() == false && controller_->isConnected() == false) {
		exit();
	} else {
		if(controller_->isConnected()) {
			// Network connection is open
			if(controller_->isUploading())
				confirmexitbox_->setInformativeText(tr("You are currently sending board contents to a new user. Please wait until it has been fully sent."));
			else
				confirmexitbox_->setInformativeText(tr("You are still connected to a drawing session."));
			confirmexitbox_->setStandardButtons(
					QMessageBox::No | QMessageBox::Cancel
					);
			confirmexitbox_->button(QMessageBox::No)->setText(
					tr("Quit and leave session")
					);
		} else {
			// Board content has changed
			confirmexitbox_->setInformativeText(tr("There are unsaved changes."));
			confirmexitbox_->setStandardButtons(
					QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel
					);
			confirmexitbox_->button(QMessageBox::No)->setText(
					tr("Quit without saving")
					);
		}
		confirmexitbox_->show();
		event->ignore();
	}
}

void MainWindow::boardChanged()
{
	setWindowModified(true);
}

void MainWindow::showNew()
{
	if(isWindowModified()) {
		disconnect(unsavedbox_, SIGNAL(finished(int)),0,0);
		connect(unsavedbox_, SIGNAL(finished(int)), this, SLOT(finishNew(int)));
		unsavedbox_->button(QMessageBox::No)->setText(tr("Discard changes"));
		unsavedbox_->show();
	} else {
		showNewDialog();
	}
}

void MainWindow::showNewDialog()
{
	QSize size = board_->sceneRect().size().toSize();
	newdlg_->setNewWidth(size.width());
	newdlg_->setNewHeight(size.height());
	newdlg_->setNewBackground(fgbgcolor_->background());
	newdlg_->show();
}

void MainWindow::newDocument()
{
	initBoard(QSize(newdlg_->newWidth(), newdlg_->newHeight()),
			newdlg_->newBackground());
	fgbgcolor_->setBackground(newdlg_->newBackground());
}

void MainWindow::open()
{
	if(isWindowModified()) {
		disconnect(unsavedbox_, SIGNAL(finished(int)),0,0);
		connect(unsavedbox_, SIGNAL(finished(int)), this, SLOT(finishOpen(int)));
		unsavedbox_->button(QMessageBox::No)->setText(tr("Discard changes"));
		unsavedbox_->show();
	} else {
		reallyOpen();
	}
}

void MainWindow::reallyOpen()
{
	// Get a list of supported formats
	QString formats;
	foreach(QByteArray format, QImageReader::supportedImageFormats()) {
		formats += "*." + format + " ";
	}
	QString filter = tr("Images (%1);;All files (*)").arg(formats);

	// Get the file name to open
	QString file = QFileDialog::getOpenFileName(this,
			tr("Open image"), lastpath_, filter);

	if(file.isEmpty()==false) {
		// Open the file
		if(initBoard(file)==false)
			showErrorMessage(ERR_OPEN);
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
		QFileInfo info(file);
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

void MainWindow::host()
{
	hostdlg_ = new dialogs::HostDialog(board_->image(), this);
	connect(hostdlg_, SIGNAL(finished(int)), this, SLOT(finishHost(int)));
	hostdlg_->show();
}

/**
 * Join action triggered, if we have unsaved changes, show a confirmation box
 * first.
 */
void MainWindow::join()
{
	if(isWindowModified()) {
		disconnect(unsavedbox_, SIGNAL(finished(int)),0,0);
		connect(unsavedbox_, SIGNAL(finished(int)), this, SLOT(initJoin(int)));
		unsavedbox_->button(QMessageBox::No)->setText(tr("Discard changes"));
		unsavedbox_->show();
	} else {
		reallyJoin();
	}
}

/**
 * Unsaved changes were confirmed or didn't exist, so display the join box
 */
void MainWindow::reallyJoin()
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
		bool useremote = hostdlg_->useRemoteAddress();
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
			address.setHost(LocalServer::address());
			// If we are not using the default port, add it to the address.
			if(hostdlg_->isDefaultPort() == false)
				address.setPort(hostdlg_->getPort());
		}

		if(address.isValid() == false || address.host().isEmpty()) {
			hostdlg_->show();
			showErrorMessage(BAD_URL);
			return;
		}
		address.setUserName(hostdlg_->getUserName());

		// Remember some settings
		QSettings cfg;
		cfg.beginGroup("network");
		cfg.setValue("username", hostdlg_->getUserName());
		if(useremote)
			cfg.setValue("remoteaddress", hostdlg_->getRemoteAddress());


		// Start server if hosting locally
		if(useremote==false) {
			LocalServer *srv = LocalServer::getInstance();
			if(srv->ensureRunning(hostdlg_->getPort())==false) {
				showErrorMessage(srv->errorString(),srv->serverOutput());
				hostdlg_->deleteLater();
				return;
			}
		}

		// If another image was selected, replace current board with it
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
			hostdlg_->getImage());
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
		QSettings cfg;
		cfg.beginGroup("network");
		cfg.setValue("username", joindlg_->getUserName());
		cfg.setValue("joinaddress", joindlg_->getAddress());

		// Connect
		joinSession(address);
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
	join_->setEnabled(false);
	logout_->setEnabled(true);
	new_->setEnabled(false);
	open_->setEnabled(false);
}

/**
 * Connection lost, so disable and enable some UI elements
 */
void MainWindow::disconnected()
{
	host_->setEnabled(true);
	join_->setEnabled(true);
	logout_->setEnabled(false);
	adminTools_->setEnabled(false);
	setSessionTitle(QString());
	new_->setEnabled(true);
	open_->setEnabled(true);
}

/**
 * @param title session title
 */
void MainWindow::joined(const QString& title)
{
	setSessionTitle(title);
	bool owner = controller_->amSessionOwner();
	userlist_->setAdminMode(owner);
	adminTools_->setEnabled(owner);
}

/**
 * Board was locked, inform the user about it.
 * @param reason the reason the board was locked
 */
void MainWindow::lock(const QString& reason)
{
	statusBar()->showMessage(tr("Board locked (%1)").arg(reason));
	lockboard_->setChecked(true);
}

/**
 * Board is no longer locked, inform the user about it.
 */
void MainWindow::unlock()
{
	statusBar()->showMessage(tr("Board unlocked"), 500);
	lockboard_->setChecked(false);
}

void MainWindow::rasterUp(int p)
{
	statusBar()->showMessage(tr("Sending board contents to new user, %1% done").arg(QString::number(p)),1000);
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
 * User decided what to do with unsaved changes before starting a new drawing
 * @param i user response
 */
void MainWindow::finishNew(int i)
{
	switch(i) {
		case QMessageBox::Save:
			if(save()==false)
				break;
		case QMessageBox::No:
			showNewDialog();
			break;
		default:
			break;
	}
}

/**
 * User decided what to do with unsaved changes before loading a new drawing.
 * @param i user response
 */
void MainWindow::finishOpen(int i)
{
	switch(i) {
		case QMessageBox::Save:
			if(save()==false)
				break;
		case QMessageBox::No:
			reallyOpen();
			break;
		default:
			break;
	}
}

/**
 * User decided what to do with unsaved changes before joining a new session.
 * Note that there is still a chance to cancel, only the join dialog is shown
 * now.
 * @param i user response
 */
void MainWindow::initJoin(int i)
{
	switch(i) {
		case QMessageBox::Save:
			if(save()==false)
				break;
		case QMessageBox::No:
			reallyJoin();
			break;
		default:
			break;
	}
}

/**
* User decided what to do with unsaved changes before exiting.
* @param i user response
*/
void MainWindow::finishExit(int i)
{
	switch(i) {
		case QMessageBox::Save:
			if(save()==false)
				break;
		case QMessageBox::No:
			if(controller_->isConnected()) {
				connect(controller_, SIGNAL(disconnected(QString)),
						this, SLOT(close()));
				controller_->disconnectHost();
			} else {
				setWindowModified(false);
				close();
			}
			break;
		default:
			break;
	}
}

/**
 * Write settings and exit.
 */
void MainWindow::exit()
{
	writeSettings();
	QApplication::quit();
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
	} else {
		return;
	}
	emit toolChanged(type);
}

void MainWindow::about()
{
	aboutdlg_->show();
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
	open_ = new QAction(QIcon(":icons/document-open.png"),tr("&Open"), this);
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
	lockboard_->setStatusTip(tr("Prevent others from making changes"));
	lockboard_->setCheckable(true);

	logout_->setEnabled(false);

	adminTools_ = new QActionGroup(this);
	adminTools_->setExclusive(false);
	adminTools_->addAction(lockboard_);
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
	pickertool_ = new QAction(QIcon(":icons/draw-picker.png"),tr("&Color picker"), this);
	pickertool_->setCheckable(true);
	pickertool_->setShortcut(QKeySequence("I"));
	zoomin_ = new QAction(QIcon(":icons/zoom-in.png"),tr("Zoom &in"), this);
	zoomin_->setShortcut(QKeySequence::ZoomIn);
	zoomout_ = new QAction(QIcon(":icons/zoom-out.png"),tr("Zoom &out"), this);
	zoomout_->setShortcut(QKeySequence::ZoomOut);
	zoomorig_ = new QAction(QIcon(":icons/zoom-original.png"),tr("&Normal size"), this);
	//zoomorig_->setShortcut(QKeySequence::ZoomOut);

	connect(zoomin_, SIGNAL(triggered()), this, SLOT(zoomin()));
	connect(zoomout_, SIGNAL(triggered()), this, SLOT(zoomout()));
	connect(zoomorig_, SIGNAL(triggered()), this, SLOT(zoomone()));

	drawingtools_ = new QActionGroup(this);
	drawingtools_->setExclusive(true);
	drawingtools_->addAction(brushtool_);
	drawingtools_->addAction(erasertool_);
	drawingtools_->addAction(pickertool_);
	connect(drawingtools_, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	// Tool cursor settings
	toggleoutline_ = new QAction(tr("Show brush &outline"), this);
	toggleoutline_->setStatusTip(tr("Display the brush outline around the cursor"));
	toggleoutline_->setCheckable(true);

	togglecrosshair_ = new QAction(tr("Crosshair c&ursor"), this);
	togglecrosshair_->setStatusTip(tr("Use a crosshair cursor"));
	togglecrosshair_->setCheckable(true);

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
	filemenu->addAction(save_);
	filemenu->addAction(saveas_);
	filemenu->addSeparator();
	filemenu->addAction(quit_);

	QMenu *viewmenu = menuBar()->addMenu(tr("&View"));
	viewmenu->addAction(toolbartoggles_);
	viewmenu->addAction(docktoggles_);
	viewmenu->addSeparator();
	viewmenu->addAction(zoomin_);
	viewmenu->addAction(zoomout_);
	viewmenu->addAction(zoomorig_);

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host_);
	sessionmenu->addAction(join_);
	sessionmenu->addAction(logout_);
	sessionmenu->addSeparator();
	sessionmenu->addAction(lockboard_);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addAction(brushtool_);
	toolsmenu->addAction(erasertool_);
	toolsmenu->addAction(pickertool_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(toggleoutline_);
	toolsmenu->addAction(togglecrosshair_);

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
	drawtools->addSeparator();
	drawtools->addAction(zoomin_);
	drawtools->addAction(zoomout_);
	drawtools->addAction(zoomorig_);
	drawtools->addSeparator();

	// Create color button
	fgbgcolor_ = new widgets::DualColorButton(drawtools);

	// Create color changer dialog for foreground
	fgdialog_ = new dialogs::ColorDialog(tr("Foreground color"), this);
	connect(fgbgcolor_,SIGNAL(foregroundClicked()), fgdialog_, SLOT(show()));
	connect(fgbgcolor_,SIGNAL(foregroundChanged(QColor)), fgdialog_, SLOT(setColor(QColor)));
	connect(fgdialog_,SIGNAL(colorChanged(QColor)), fgbgcolor_, SLOT(setForeground(QColor)));

	// Create color changer dialog for background
	bgdialog_ = new dialogs::ColorDialog(tr("Background color"), this);
	connect(fgbgcolor_,SIGNAL(backgroundClicked()), bgdialog_, SLOT(show()));
	connect(fgbgcolor_,SIGNAL(backgroundChanged(QColor)), bgdialog_, SLOT(setColor(QColor)));
	connect(bgdialog_,SIGNAL(colorChanged(QColor)), fgbgcolor_, SLOT(setBackground(QColor)));

	drawtools->addWidget(fgbgcolor_);

	addToolBar(Qt::LeftToolBarArea, drawtools);

	toolbartoggles_->setMenu(togglemenu);
}

void MainWindow::createDocks()
{
	QMenu *toggles = new QMenu(this);
	createToolSettings(toggles);
	createUserList(toggles);
	docktoggles_->setMenu(toggles);
}

void MainWindow::createToolSettings(QMenu *toggles)
{
	toolsettings_ = new widgets::ToolSettings(this);
	toolsettings_->setObjectName("toolsettingsdock");
	toolsettings_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
#ifdef Q_WS_WIN
	// Dock widget floating is buggy on Windows.
	toolsettings_->setFeatures(toolsettings_->features() & ~QDockWidget::DockWidgetFloatable);
#endif
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
#ifdef Q_WS_WIN
	// Dock widget floating is buggy on Windows.
	userlist_->setFeatures(userlist_->features() & ~QDockWidget::DockWidgetFloatable);
#endif
	toggles->addAction(userlist_->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, userlist_);
}

void MainWindow::createDialogs()
{
	aboutdlg_ = new dialogs::AboutDialog(this);
	newdlg_ = new dialogs::NewDialog(this);
	connect(newdlg_, SIGNAL(accepted()), this, SLOT(newDocument()));

	msgbox_ = new QMessageBox(this);
	msgbox_->setWindowTitle(tr("DrawPile"));
	msgbox_->setWindowModality(Qt::WindowModal);
	msgbox_->setWindowFlags(msgbox_->windowFlags() | Qt::Sheet);

	confirmexitbox_ = new QMessageBox(QMessageBox::Warning,
			tr("DrawPile"),
			tr("Really exit DrawPile?"),
                QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel,
                this, Qt::MSWindowsFixedSizeDialogHint|Qt::Sheet);


	connect(confirmexitbox_, SIGNAL(finished(int)), this, SLOT(finishExit(int)));

	unsavedbox_ = new QMessageBox(QMessageBox::Warning,
			tr("DrawPile"),
			tr("The drawing has been modified.\n"
				"Do you want to save your changes?"),
                QMessageBox::Save | QMessageBox::No | QMessageBox::Cancel,
                this, Qt::MSWindowsFixedSizeDialogHint|Qt::Sheet);

	leavebox_ = new QMessageBox(QMessageBox::Question,
			tr("DrawPile"),
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

