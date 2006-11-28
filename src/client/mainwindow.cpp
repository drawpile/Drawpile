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

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QSettings>
#include <QFileDialog>

#include "mainwindow.h"
#include "netstatus.h"
#include "hostlabel.h"
#include "dualcolorbutton.h"
#include "editorview.h"
#include "board.h"
#include "controller.h"
#include "toolsettingswidget.h"

MainWindow::MainWindow()
	: QMainWindow()
{
	setWindowTitle(tr("DrawPile"));

	initActions();
	createMenus();
	createToolbars();
	createDocks();

	QStatusBar *statusbar = new QStatusBar(this);
	setStatusBar(statusbar);

	hostaddress_ = new widgets::HostLabel();
	statusbar->addPermanentWidget(hostaddress_);

	netstatus_ = new widgets::NetStatus(this);
	statusbar->addPermanentWidget(netstatus_);

	view_ = new widgets::EditorView(this);
	setCentralWidget(view_);

	board_ = new drawingboard::Board(this);
	board_->setBackgroundBrush(
			palette().brush(QPalette::Active,QPalette::Window));
	board_->initBoard(QSize(800,600),Qt::white);
	view_->setScene(board_);

	controller_ = new Controller(this);
	controller_->setBoard(board_);
	controller_->setColors(fgbgcolor_);
	controller_->setSettings(toolsettings_);

	connect(view_,SIGNAL(penDown(int,int,qreal,bool)),controller_,SLOT(penDown(int,int,qreal,bool)));
	connect(view_,SIGNAL(penMove(int,int,qreal)),controller_,SLOT(penMove(int,int,qreal)));
	connect(view_,SIGNAL(penUp()),controller_,SLOT(penUp()));

	readSettings();
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
	int tool = cfg.value("tool", 0).toInt();
	QList<QAction*> actions = drawingTools_->actions();
	if(tool<0 || tool>=actions.count()) tool=0;
	actions[tool]->trigger();
	toolsettings_->setTool(tools::Type(tool));
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
	int tool = drawingTools_->actions().indexOf(drawingTools_->checkedAction());
	cfg.setValue("tool", tool);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
	(void)event;
	writeSettings();
}

void MainWindow::save()
{
	if(filename_.isEmpty()) {
		saveas();
	} else {
		board_->save(filename_);
	}
}

void MainWindow::saveas()
{
	QString file = QFileDialog::getSaveFileName(this,
			tr("Save image"), lastpath_,
			tr("Images (*.png *.jpg *.bmp)"));
	if(file.isEmpty()==false) {
		QFileInfo info(file);
		lastpath_ = info.absolutePath();
		filename_ = info.absoluteFilePath();
		if(info.suffix().isEmpty())
			filename_ += ".png";
		board_->save(filename_);
	}
}

void MainWindow::zoomin()
{
	view_->scale(2.0,2.0);
}

void MainWindow::zoomout()
{
	view_->scale(0.5,0.5);
}

void MainWindow::zoomone()
{
	view_->resetMatrix();
}

void MainWindow::selectTool(QAction *tool)
{
	tools::Type type;
	if(tool == brushTool_) {
		type = tools::BRUSH;
	} else if(tool == eraserTool_) {
		type = tools::ERASER;
	} else {
		return;
	}
	emit toolChanged(type);
}

void MainWindow::initActions()
{
	// File actions
	save_ = new QAction(QIcon(":icons/document-save.png"),tr("&Save"), this);
	save_->setShortcut(QKeySequence::Save);
	save_->setStatusTip(tr("Save picture to file"));
	saveas_ = new QAction(QIcon(":icons/document-save-as.png"),tr("Save &As..."), this);
	saveas_->setStatusTip(tr("Save picture to file with a new name"));
	quit_ = new QAction(QIcon(":icons/system-log-out.png"),tr("&Quit"), this);
	quit_->setStatusTip(tr("Quit the program"));
	quit_->setShortcut(QKeySequence("Ctrl+Q"));
	quit_->setMenuRole(QAction::QuitRole);

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
	brushTool_ = new QAction(QIcon(":icons/draw-brush.png"),tr("Brush"), this);
	brushTool_->setCheckable(true); brushTool_->setChecked(true);
	eraserTool_ = new QAction(QIcon(":icons/draw-eraser.png"),tr("Eraser"), this);
	eraserTool_->setCheckable(true);
	zoomin_ = new QAction(QIcon(":icons/zoom-in.png"),tr("Zoom in"), this);
	zoomin_->setShortcut(QKeySequence::ZoomIn);
	zoomout_ = new QAction(QIcon(":icons/zoom-out.png"),tr("Zoom out"), this);
	zoomout_->setShortcut(QKeySequence::ZoomOut);
	zoomorig_ = new QAction(QIcon(":icons/zoom-original.png"),tr("Actual size"), this);
	//zoomorig_->setShortcut(QKeySequence::ZoomOut);

	connect(zoomin_, SIGNAL(triggered()), this, SLOT(zoomin()));
	connect(zoomout_, SIGNAL(triggered()), this, SLOT(zoomout()));
	connect(zoomorig_, SIGNAL(triggered()), this, SLOT(zoomone()));

	drawingTools_ = new QActionGroup(this);
	drawingTools_->setExclusive(true);
	drawingTools_->addAction(brushTool_);
	drawingTools_->addAction(eraserTool_);
	connect(drawingTools_, SIGNAL(triggered(QAction*)), this, SLOT(selectTool(QAction*)));

	// Toolbar toggling actions
	toolbartoggles_ = new QAction(tr("Toolbars"), this);
	docktoggles_ = new QAction(tr("Docks"), this);

	// Help actions
	help_ = new QAction(tr("DrawPile Help"), this);
	help_->setShortcut(QKeySequence("F1"));
	about_ = new QAction(tr("About DrawPile"), this);
	about_->setMenuRole(QAction::AboutRole);
}

void MainWindow::createMenus()
{
	QMenu *filemenu = menuBar()->addMenu(tr("&File"));
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

	QMenu *settingsmenu = menuBar()->addMenu(tr("Settings"));

	QMenu *windowmenu = menuBar()->addMenu(tr("&Window"));
	windowmenu->addAction(toolbartoggles_);
	windowmenu->addAction(docktoggles_);
	windowmenu->addSeparator();
	windowmenu->addAction(zoomin_);
	windowmenu->addAction(zoomout_);
	windowmenu->addAction(zoomorig_);

	QMenu *helpmenu = menuBar()->addMenu(tr("&Help"));
	helpmenu->addAction(help_);
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
	filetools->addAction(save_);
	filetools->addAction(saveas_);
	addToolBar(Qt::TopToolBarArea, filetools);

	// Drawing toolbar
	QToolBar *drawtools = new QToolBar("Drawing tools");
	drawtools->setObjectName("drawtoolsbar");
	togglemenu->addAction(drawtools->toggleViewAction());

	drawtools->addAction(brushTool_);
	drawtools->addAction(eraserTool_);
	drawtools->addSeparator();
	drawtools->addAction(zoomin_);
	drawtools->addAction(zoomout_);
	drawtools->addAction(zoomorig_);
	drawtools->addSeparator();

	fgbgcolor_ = new widgets::DualColorButton(drawtools);
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
	connect(this, SIGNAL(toolChanged(tools::Type)), toolsettings_, SLOT(setTool(tools::Type)));
	toggles->addAction(toolsettings_->toggleViewAction());
	addDockWidget(Qt::RightDockWidgetArea, toolsettings_);
}

