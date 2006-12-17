/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

#include "mainwindow.h"
#include "netstatus.h"
#include "hostlabel.h"
#include "aboutdialog.h"
#include "dualcolorbutton.h"
#include "editorview.h"
#include "board.h"
#include "controller.h"
#include "toolsettingswidget.h"
#include "colordialog.h"
#include "newdialog.h"

MainWindow::MainWindow()
	: QMainWindow()
{
	setTitle();

	initActions();
	createMenus();
	createToolbars();
	createDocks();

	aboutdlg_ = new dialogs::AboutDialog(this);
	newdlg_ = new dialogs::NewDialog(this);
	connect(newdlg_, SIGNAL(accepted()), this, SLOT(newDocument()));
	msgbox_ = new QMessageBox(this);
	msgbox_->setWindowTitle(tr("DrawPile"));
	msgbox_->setWindowModality(Qt::WindowModal);
	msgbox_->setWindowFlags(msgbox_->windowFlags() | Qt::Sheet);

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	hostaddress_ = new widgets::HostLabel();
	statusbar->addPermanentWidget(hostaddress_);

	netstatus_ = new widgets::NetStatus(this);
	statusbar->addPermanentWidget(netstatus_);

	view_ = new widgets::EditorView(this);
	connect(toolsettings_, SIGNAL(sizeChanged(int)), view_, SLOT(setOutlineRadius(int)));
	connect(toggleoutline_, SIGNAL(triggered(bool)), view_, SLOT(setOutline(bool)));
	connect(togglecrosshair_, SIGNAL(triggered(bool)), view_, SLOT(setCrosshair(bool)));
	setCentralWidget(view_);

	board_ = new drawingboard::Board(this);
	board_->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	board_->initBoard(QSize(800,600),Qt::white);
	view_->setBoard(board_);

	controller_ = new Controller(this);
	controller_->setModel(board_, toolsettings_, fgbgcolor_);
	connect(this, SIGNAL(toolChanged(tools::Type)), controller_, SLOT(setTool(tools::Type)));

	connect(view_,SIGNAL(penDown(int,int,qreal,bool)),controller_,SLOT(penDown(int,int,qreal,bool)));
	connect(view_,SIGNAL(penMove(int,int,qreal)),controller_,SLOT(penMove(int,int,qreal)));
	connect(view_,SIGNAL(penUp()),controller_,SLOT(penUp()));

	readSettings();
}

void MainWindow::setTitle()
{
	setWindowTitle(tr("DrawPile[*] - %1").arg(filename_.isEmpty()?tr("Untitled"):filename_));
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

void MainWindow::closeEvent(QCloseEvent *event)
{
	(void)event;
	writeSettings();
	QApplication::quit();
}

void MainWindow::showNewDialog()
{
	newdlg_->show();
}

void MainWindow::newDocument()
{
	board_->initBoard(QSize(newdlg_->newWidth(), newdlg_->newHeight()),
			newdlg_->newBackground());
	filename_ = "";
	setTitle();
}

void MainWindow::open()
{
	QString file = QFileDialog::getOpenFileName(this,
			tr("Open image"), lastpath_,
			tr("Images (*.png *.jpg *.bmp);;All files (*)"));
	if(file.isEmpty()==false) {
		QImage img(file);
		if(img.isNull()) {
			showErrorMessage(ERR_OPEN);
		} else {
			board_->initBoard(img);
			filename_ = file;
			setTitle();
		}
	}
}

/**
 * If no file name has been selected, \a saveas is called.
 */
void MainWindow::save()
{
	if(filename_.isEmpty()) {
		saveas();
	} else {
		if(board_->save(filename_) == false)
			showErrorMessage(ERR_SAVE);
	}
}

/**
 * A standard file dialog is used to get the name of the file to save.
 * If no suffix is entered, ".png" is used.
 */
void MainWindow::saveas()
{
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save image"), lastpath_,
			tr("Images (*.png *.jpg *.bmp)"));
	if(file.isEmpty()==false) {
		QFileInfo info(file);
		lastpath_ = info.absolutePath();
		if(info.suffix().isEmpty())
			file += ".png";
		if(board_->save(file) == false) {
			showErrorMessage(ERR_SAVE);
		} else {
			filename_ = file;
			setTitle();
		}
	}
}

void MainWindow::showErrorMessage(ErrorType type)
{
	QString msg;
	switch(type) {
		case ERR_SAVE: msg = tr("An error occured while trying to save the image"); break;
		case ERR_OPEN: msg = tr("An error occured while trying to open the image"); break;
		default: qFatal("no such error type");
	}

	msgbox_->setStandardButtons(QMessageBox::Ok);
	msgbox_->setIcon(QMessageBox::Warning);
	msgbox_->setText(msg);
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
 * 
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

	connect(new_,SIGNAL(triggered()), this, SLOT(showNewDialog()));
	connect(open_,SIGNAL(triggered()), this, SLOT(open()));
	connect(save_,SIGNAL(triggered()), this, SLOT(save()));
	connect(saveas_,SIGNAL(triggered()), this, SLOT(saveas()));
	connect(quit_,SIGNAL(triggered()), this, SLOT(close()));

	// Session actions
	host_ = new QAction("Host...", this);
	host_->setStatusTip(tr("Host a new drawing session"));
	join_ = new QAction("Join...", this);
	join_->setStatusTip(tr("Join an existing drawing session"));
	logout_ = new QAction("Leave", this);
	logout_->setStatusTip(tr("Leave this drawing session"));
	lockboard_ = new QAction("Lock the board", this);
	lockboard_->setStatusTip(tr("Prevent others from making changes"));
	kickuser_ = new QAction("Kick", this);
	lockuser_ = new QAction("Lock", this);

	adminTools_ = new QActionGroup(this);
	adminTools_->addAction(lockboard_);
	adminTools_->addAction(kickuser_);
	adminTools_->addAction(lockuser_);

	// Drawing tool actions
	brushtool_ = new QAction(QIcon(":icons/draw-brush.png"),tr("Brush"), this);
	brushtool_->setCheckable(true); brushtool_->setChecked(true);
	erasertool_ = new QAction(QIcon(":icons/draw-eraser.png"),tr("Eraser"), this);
	erasertool_->setCheckable(true);
	pickertool_ = new QAction(QIcon(":icons/draw-picker.png"),tr("Color picker"), this);
	pickertool_->setCheckable(true);
	zoomin_ = new QAction(QIcon(":icons/zoom-in.png"),tr("Zoom in"), this);
	zoomin_->setShortcut(QKeySequence::ZoomIn);
	zoomout_ = new QAction(QIcon(":icons/zoom-out.png"),tr("Zoom out"), this);
	zoomout_->setShortcut(QKeySequence::ZoomOut);
	zoomorig_ = new QAction(QIcon(":icons/zoom-original.png"),tr("Actual size"), this);
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
	toggleoutline_ = new QAction(tr("Show brush outline"), this);
	toggleoutline_->setCheckable(true);

	togglecrosshair_ = new QAction(tr("Crosshair cursor"), this);
	togglecrosshair_->setCheckable(true);

	// Toolbar toggling actions
	toolbartoggles_ = new QAction(tr("Toolbars"), this);
	docktoggles_ = new QAction(tr("Docks"), this);

	// Help actions
	help_ = new QAction(tr("DrawPile Help"), this);
	help_->setShortcut(QKeySequence("F1"));
	help_->setEnabled(false);
	connect(help_,SIGNAL(triggered()), this, SLOT(help()));
	homepage_ = new QAction(tr("DrawPile homepage"), this);
	connect(homepage_,SIGNAL(triggered()), this, SLOT(homepage()));
	about_ = new QAction(tr("About DrawPile"), this);
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

	QMenu *sessionmenu = menuBar()->addMenu(tr("&Session"));
	sessionmenu->addAction(host_);
	sessionmenu->addAction(join_);
	sessionmenu->addAction(logout_);
	sessionmenu->addSeparator();
	sessionmenu->addAction(lockboard_);
	sessionmenu->addAction(lockuser_);
	sessionmenu->addAction(kickuser_);

	QMenu *toolsmenu = menuBar()->addMenu(tr("&Tools"));
	toolsmenu->addAction(brushtool_);
	toolsmenu->addAction(erasertool_);
	toolsmenu->addAction(pickertool_);
	toolsmenu->addSeparator();
	toolsmenu->addAction(toggleoutline_);
	toolsmenu->addAction(togglecrosshair_);

	//QMenu *settingsmenu = menuBar()->addMenu(tr("Settings"));

	QMenu *windowmenu = menuBar()->addMenu(tr("&Window"));
	windowmenu->addAction(toolbartoggles_);
	windowmenu->addAction(docktoggles_);
	windowmenu->addSeparator();
	windowmenu->addAction(zoomin_);
	windowmenu->addAction(zoomout_);
	windowmenu->addAction(zoomorig_);

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

