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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "tools.h"

class QActionGroup;
class QMessageBox;
class QUrl;
class QLabel;
class QSplitter;
class QTimer;

namespace widgets {
	class CanvasView;
	class DualColorButton;
	class ToolSettings;
	class UserList;
	class LayerListDock;
	class PaletteBox;
	class ColorBox;
	class Navigator;
}
namespace dialogs {
	class ColorDialog;
	class HostDialog;
	class JoinDialog;
}
namespace drawingboard {
	class CanvasScene;
}

namespace net {
	class Client;
}

class SessionLoader;

//! The application main window
class MainWindow : public QMainWindow {
	Q_OBJECT
	public:
		MainWindow(bool restoreWindowPosition=true);
		~MainWindow();

		MainWindow *loadDocument(SessionLoader &loader);

		//! Connect to a host and join a session if full URL is provided.
		void joinSession(const QUrl& url);

		//! React to eraser tip proximity
		void eraserNear(bool near);

	public slots:
		// Triggerable actions
		void showNew();
		void open();
		void open(const QString& file);
		bool save();
		bool saveas();

		void showSettings();
		void changeSessionTitle();

		void host();
		void join();
		void leave();

		void zoomin();
		void zoomout();
		void zoomone();
		void rotatezero();

		void fullscreen(bool enable);
		void setShowAnnotations(bool show);

		void selectTool(QAction *tool);

		void about();
		void homepage();

	private slots:
		void setSessionTitle(const QString& title);
		void setOperatorMode(bool op);

		void newDocument(const QSize &size, const QColor &color);

		void finishHost(int i);
		void finishJoin(int i);

		void connecting();
		void loggedin(bool join);
		void disconnected(const QString &message);
		void sessionConfChanged(bool locked, bool closed);

		void updateLockWidget();

		void updateShortcuts();

		void copyVisible();
		void copyLayer();
		void paste();

	signals:
		//! This signal is emitted when the current tool is changed
		void toolChanged(tools::Type);

	protected:
		//! Handle closing of the main window
		void closeEvent(QCloseEvent *event);

	private:
		//! Confirm saving of image in a format that doesn't support all required features
		bool confirmFlatten(QString& file) const;

		QAction *makeAction(const char *name, const char *icon, const QString& text, const QString& tip = QString(), const QKeySequence& shortcut = QKeySequence(), bool checkable=false);
		QAction *getAction(const QString &name);

		//! Load customized shortcuts
		void loadShortcuts();

		//! Check if the current board can be replaced
		bool canReplace() const;

		//! Add a new entry to recent files list
		void addRecentFile(const QString& file);

		//! Set the window title according to open file name
		void updateTitle();

		//! Save settings and exit
		void exit();

		//! Display an error message
		void showErrorMessage(const QString& message, const QString& details=QString());

		void readSettings(bool windowpos=true);
		void writeSettings();

		void createDocks();
		void setupActions();

		QSplitter *splitter_;
		widgets::ToolSettingsDock *_toolsettings;
		widgets::UserList *_userlist;
		widgets::LayerListDock *_layerlist;

		widgets::DualColorButton *_fgbgcolor;
		widgets::CanvasView *_view;
		widgets::PaletteBox *palette_;
		widgets::ColorBox *rgb_, *hsv_;
		widgets::Navigator *navigator_;
		QLabel *_lockstatus;

		dialogs::ColorDialog *_fgdialog, *_bgdialog;
		dialogs::HostDialog *hostdlg_;
		dialogs::JoinDialog *joindlg_;

		drawingboard::CanvasScene *_canvas;
		net::Client *_client;

		QString filename_;
		QString lastpath_;
		QMenu *_recent;

		QActionGroup *_currentdoctools; // actions relating to the currently open document
		QActionGroup *_admintools; // session operator actions
		QActionGroup *_drawingtools; // drawing tool selection
		QAction *_lasttool; // the last used tool

		// List of customizeable actions
		QList<QAction*> customacts_;

		// Remember window state to return from fullscreen mode
		QByteArray _fullscreen_oldstate;
		QRect _fullscreen_oldgeometry;
};

#endif

