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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "tools.h"

class QActionGroup;
class QMessageBox;
class QUrl;

namespace widgets {
	class NetStatus;
	class EditorView;
	class DualColorButton;
	class ToolSettings;
}
namespace dialogs {
	class ColorDialog;
	class NewDialog;
	class HostDialog;
	class JoinDialog;
	class LoginDialog;
}
namespace drawingboard {
	class Board;
}
class Controller;

//! The application main window
class MainWindow : public QMainWindow {
	Q_OBJECT
	enum ErrorType {ERR_SAVE, ERR_OPEN, BAD_URL};
	public:
		MainWindow();

		//! Initialize the drawing board from an existing image
		void initBoard(const QImage& image);

		//! Initialize the drawing board from a file
		bool initBoard(const QString& filename);

		//! Initialize a blank drawing board
		void initBoard(const QSize& size, const QColor& color);

		//! Connect to a host and join a session if full URL is provided.
		void joinSession(const QUrl& url);

	public slots:
		//! Check if document is changed and show New dialog
		void showNew();
		//! Show file open dialog
		void open();
		//! Save current document
		bool save();
		//! Save current document with a new name
		bool saveas();
		//! Show host session dialog
		void host();
		//! Show join session dialog
		void join();
		//! Leave session (ask confirmation first)
		void leave();
		//! Zoom in
		void zoomin();
		//! Zoom out
		void zoomout();
		//! Reset to 1:1 zoom
		void zoomone();
		//! Change current tool
		void selectTool(QAction *tool);
		//! Display about dialog
		void about();
		//! Display online help
		void help();
		//! Go to drawpile homepage
		void homepage();
	private slots:
		//! Set session title
		void setSessionTitle(const QString& title);
		//! Create new document
		void newDocument();
		//! Show new document dialog
		void showNewDialog();
		//! Mark unsaved changes
		void boardChanged();
		//! Save and create a new image
		void finishNew(int i);
		//! Save and open another image
		void finishOpen(int i);
		//! Save and exit
		void finishExit(int i);
		//! Cancel or start hosting
		void finishHost(int i);
		//! Show join dialog
		void initJoin(int i);
		//! Cancel or join
		void finishJoin(int i);
		//! Leave session
		void finishLeave(int i);
		//! Logged in, host session
		void loggedinHost();
		//! Logged in, join session
		void loggedinJoin();
		//! Connection established
		void connected();
		//! Connection cut
		void disconnected();
		//! Disallow changes to the board
		void lock(const QString& reason);
		//! Allow changes to the board
		void unlock();
		//! Inform user about raster upload progress
		void rasterUp(int p);

	signals:
		//! This signal is emitted when the current tool is changed
		void toolChanged(tools::Type);

	protected:
		//! Handle closing of the main window
		void closeEvent(QCloseEvent *event);

	private:
		//! Set the window title according to open file name
		void setTitle();

		//! Show join dialog without checking for unsaved changes
		void reallyJoin();

		//! Show open dialog without checking for unsaved changes
		void reallyOpen();

		//! Save settings and exit
		void exit();

		//! Display a standardised error message
		void showErrorMessage(ErrorType type);

		//! Display an error message
		void showErrorMessage(const QString& message);

		//! Read settings from file/registry
		void readSettings();
		//! Write settings to file/reqistry
		void writeSettings();

		//! Initialise QActions
		void initActions();
		//! Create menus
		void createMenus();
		//! Create toolbars
		void createToolbars();
		//! Create all dock windows
		void createDocks();
		//! Create tool settings dock
		void createToolSettings(QMenu *menu);
		//! Create dialogs
		void createDialogs();

		widgets::ToolSettings *toolsettings_;
		widgets::DualColorButton *fgbgcolor_;
		widgets::NetStatus *netstatus_;
		widgets::EditorView *view_;

		dialogs::ColorDialog *fgdialog_,*bgdialog_;
		QDialog *aboutdlg_;
		dialogs::NewDialog *newdlg_;
		dialogs::HostDialog *hostdlg_;
		dialogs::JoinDialog *joindlg_;
		dialogs::LoginDialog *logindlg_;
		QMessageBox *msgbox_;
		QMessageBox *unsavedbox_;
		QMessageBox *leavebox_;

		drawingboard::Board *board_;
		Controller *controller_;

		QString sessiontitle_;
		QString filename_;
		QString lastpath_;

		QAction *new_;
		QAction *open_;
		QAction *save_;
		QAction *saveas_;
		QAction *quit_;

		QAction *host_;
		QAction *join_;
		QAction *logout_;

		QActionGroup *adminTools_;
		QAction *lockboard_;
		QAction *lockuser_;
		QAction *kickuser_;

		QActionGroup *drawingtools_;
		QAction *brushtool_;
		QAction *erasertool_;
		QAction *pickertool_;
		QAction *zoomin_;
		QAction *zoomout_;
		QAction *zoomorig_;

		QAction *toggleoutline_;
		QAction *togglecrosshair_;

		QAction *toolbartoggles_;
		QAction *docktoggles_;

		QAction *help_;
		QAction *homepage_;
		QAction *about_;
};

#endif

