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
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QElapsedTimer>

#include "tools/tool.h"

class QActionGroup;
class QMessageBox;
class QUrl;
class QLabel;
class QSplitter;
class QTimer;

namespace widgets {
	class CanvasView;
	class NetStatus;
	class ChatBox;
}
namespace docks {
	class ToolSettings;
	class InputSettings;
	class UserList;
	class LayerList;
	class PaletteBox;
	class ColorBox;
	class Navigator;
}
namespace dialogs {
	class PlaybackDialog;
	class HostDialog;
}
namespace drawingboard {
	class CanvasScene;
}

namespace net {
	class Client;
}

namespace recording {
	class Writer;
	class Reader;
}

class SessionLoader;
class ShortcutDetector;

//! The application main window
class MainWindow : public QMainWindow {
	Q_OBJECT
	public:
		MainWindow(bool restoreWindowPosition=true);
		~MainWindow();

		MainWindow *loadDocument(SessionLoader &loader);
		MainWindow *loadRecording(recording::Reader *reader);

		//! Host a session using the settings from the given dialog
		void hostSession(dialogs::HostDialog *dlg);

		//! Connect to a host and join a session if full URL is provided.
		void joinSession(const QUrl& url, bool autoRecord=false);

		//! Check if the current board can be replaced
		bool canReplace() const;

		//! Save settings and exit
		void exit();

	public slots:
		// Triggerable actions
		void showNew();
		void open();
		void open(const QUrl &url);
		bool save();
		bool saveas();

		static void showSettings();
		void changeSessionTitle();

		void host();
		void join();
		void leave();

		void toggleFullscreen();
		void setShowAnnotations(bool show);
		void setShowLaserTrails(bool show);

		void selectTool(QAction *tool);

		static void about();
		static void homepage();

		//! Create a blank new document
		void newDocument(const QSize &size, const QColor &background);

	private slots:
		void toggleRecording();

		void setSessionTitle(const QString& title);
		void setOperatorMode(bool op);

		void connecting();
		void loggedin(bool join);
		void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
		void sessionConfChanged(bool locked, bool layerctrllocked, bool closed, bool preservechat);

		void updateLockWidget();
		void setRecorderStatus(bool on);
		void statusbarChat(const QString &nick, const QString &msg);

		void updateShortcuts();
		void updateTabletSupportMode();

		void selectAll();
		void selectNone();
		void copyVisible();
		void copyLayer();
		void cutLayer();
		void paste();
		void pasteFile();
		void pasteFile(const QUrl &url);
		void pasteImage(const QImage &image);
		void dropUrl(const QUrl &url);

		void clearOrDelete();
		void fillFgArea();
		void fillBgArea();

		void removeEmptyAnnotations();
		void resizeCanvas();
		void markSpotForRecording();

		void toolChanged(tools::Type tool);

		void hotBorderMenubar(bool show);

	protected:
		//! Handle closing of the main window
		void closeEvent(QCloseEvent *event);

		bool event(QEvent *event);

	private:
		//! Confirm saving of image in a format that doesn't support all required features
		bool confirmFlatten(QString& file) const;

		QAction *makeAction(const char *name, const char *icon, const QString& text, const QString& tip = QString(), const QKeySequence& shortcut = QKeySequence(), bool checkable=false);
		QAction *getAction(const QString &name);

		//! Load customized shortcuts
		void loadShortcuts();

		//! Add a new entry to recent files list
		void addRecentFile(const QString& file);

		//! Set the window title according to open file name
		void updateTitle();

		//! Enable or disable drawing tools
		void setDrawingToolsEnabled(bool enable);

		//! Display an error message
		void showErrorMessage(const QString& message, const QString& details=QString());

		void startRecorder(const QString &filename);

		void readSettings(bool windowpos=true);
		void writeSettings();

		void createDocks();
		void setupActions();

		void fillArea(const QColor &color);

		QSplitter *_splitter;

		docks::ToolSettings *_dock_toolsettings;
		docks::InputSettings *_dock_input;
		docks::UserList *_dock_users;
		docks::LayerList *_dock_layers;
		docks::ColorBox *_dock_colors;
		docks::Navigator *_dock_navigator;
		widgets::ChatBox *_chatbox;

		widgets::CanvasView *_view;

		QStatusBar *_viewStatusBar;
		QLabel *_lockstatus;
		QLabel *_recorderstatus;
		widgets::NetStatus *_netstatus;

		dialogs::PlaybackDialog *_dialog_playback;

		drawingboard::CanvasScene *_canvas;
		net::Client *_client;

		QString _current_filename;
		QMenu *_recent;

		recording::Writer *_recorder;
		bool _autoRecordOnConnect;

		QActionGroup *_currentdoctools; // actions relating to the currently open document
		QActionGroup *_admintools; // session operator actions
		QActionGroup *_docadmintools; // current document related operator actions
		QActionGroup *_drawingtools; // drawing tool selection
		QActionGroup *_toolslotactions; // tool slot selection

		// Remember window state to return from fullscreen mode
		QByteArray _fullscreen_oldstate;
		QRect _fullscreen_oldgeometry;

		QElapsedTimer _toolChangeTime; // how long the user has held down the tool change button
		ShortcutDetector *_tempToolSwitchShortcut;
};

#endif

