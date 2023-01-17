/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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

extern "C" {
#include <dpengine/load.h>
}

#include <QMainWindow>
#include <QElapsedTimer>
#include <QUrl>
#include <QPointer>

#include "tools/tool.h"
#include "canvas/acl.h"
#include "export/animationsaverrunnable.h"

class QActionGroup;
class QLabel;
class QSplitter;
class QToolButton;

class Document;
class ActionBuilder;

namespace widgets {
	class CanvasView;
	class NetStatus;
	class ChatBox;
	class ViewStatus;
}
namespace docks {
	class ToolSettings;
	class BrushPalette;
	class InputSettings;
	class LayerList;
	class PaletteBox;
	class ColorPaletteDock;
	class ColorSpinnerDock;
	class ColorSliderDock;
	class Navigator;
	class OnionSkinsDock;
	class Timeline;
}
namespace dialogs {
	class PlaybackDialog;
	class HostDialog;
	class SessionSettingsDialog;
	class ServerLogDialog;
}
namespace drawingboard {
	class CanvasScene;
}
namespace canvas {
	class CanvasModel;
}
namespace rustpile {
	enum class CanvasIoError;
	enum class AnimationExportMode;
}

class ShortcutDetector;

//! The application main window
class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	MainWindow(bool restoreWindowPosition=true);
	~MainWindow();

	MainWindow *loadRecording(const QString &path);

	//! Host a session using the settings from the given dialog
	void hostSession(dialogs::HostDialog *dlg);

	//! Connect to a host and join a session if full URL is provided.
	void joinSession(const QUrl& url, const QString &autoRecordFilename=QString());

	//! Check if the current board can be replaced
	bool canReplace() const;

	//! Save settings and exit
	void exit();

public slots:
	// Triggerable actions
	void showNew();
	void open();
	void open(const QUrl &url);
	void save();
	void saveas();
	void saveSelection();
	void showFlipbook();

	static void showSettings();
	void reportAbuse();
	void tryToGainOp();
	void resetSession();
	void terminateSession();

	void host();
	void join(const QUrl &defaultUrl=QUrl());
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
	void toggleProfile();

	void exportGifAnimation();
	void exportAnimationFrames();

	void onOperatorModeChange(bool op);
	void onFeatureAccessChange(DP_Feature feature, bool canUse);

	void onServerConnected();
	void onServerLogin();
	void onServerDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
	void onNsfmChanged(bool nsfm);

	void updateLockWidget();
	void setRecorderStatus(bool on);

	void loadShortcuts();
	void updateSettings();

	void toggleLayerViewMode();
	void updateLayerViewMode();

	void copyText();
	void paste();
	void pasteFile();
	void pasteFile(const QUrl &url);
	void pasteImage(const QImage &image, const QPoint *point=nullptr);
	void dropUrl(const QUrl &url);

	void clearOrDelete();

	void resizeCanvas();
	void updateBackgroundActions();
	void changeCanvasBackground();
	void changeLocalCanvasBackground();
	void clearLocalCanvasBackground();

	void toolChanged(tools::Tool::Type tool);

	void selectionRemoved();

	void hotBorderMenubar(bool show);

	void setFreezeDocks(bool freeze);
	void setDocksHidden(bool hidden);

	void updateTitle();

	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onCanvasSaveStarted();
	void onCanvasSaved(const QString &errorMessage);

protected:
	void closeEvent(QCloseEvent *event);
	bool event(QEvent *event);

private:
	MainWindow *replaceableWindow();

	//! Confirm saving of image in a format that doesn't support all required features
	bool confirmFlatten(QString& file) const;

	void exportAnimation(const QString &path, AnimationSaverRunnable::SaveFn saveFn);

	ActionBuilder makeAction(const char *name, const QString &text);
	QAction *getAction(const QString &name);

	//! Add a new entry to recent files list
	void addRecentFile(const QString& file);

	//! Enable or disable drawing tools
	void setDrawingToolsEnabled(bool enable);

	//! Display an error message
	void showErrorMessage(const QString &message);
	void showErrorMessageWithDetails(const QString &message, const QString &details);
	void showLoadResultMessage(DP_LoadResult result);

	void readSettings(bool windowpos=true);
	void writeSettings();

	void createDocks();
	void setupActions();

	QSplitter *m_splitter;

	docks::ToolSettings *m_dockToolSettings;
	docks::BrushPalette *m_dockBrushPalette;
	docks::InputSettings *m_dockInput;
	docks::LayerList *m_dockLayers;
	docks::ColorPaletteDock *m_dockColorPalette;
	docks::ColorSpinnerDock *m_dockColorSpinner;
	docks::ColorSliderDock *m_dockColorSliders;
	docks::Navigator *m_dockNavigator;
	docks::OnionSkinsDock *m_dockOnionSkins;
	docks::Timeline *m_dockTimeline;
	widgets::ChatBox *m_chatbox;

	widgets::CanvasView *m_view;

	QStatusBar *m_viewStatusBar;
	QLabel *m_lockstatus;
	widgets::NetStatus *m_netstatus;
	widgets::ViewStatus *m_viewstatus;
	QToolButton *m_statusChatButton;

	QPointer<dialogs::PlaybackDialog> m_playbackDialog;
	dialogs::SessionSettingsDialog *m_sessionSettings;
	dialogs::ServerLogDialog *m_serverLogDialog;

	drawingboard::CanvasScene *m_canvasscene;

	QMenu *m_recentMenu;
	QAction *m_lastLayerViewMode;

	QActionGroup *m_currentdoctools; // general tools that require no special permissions
	QActionGroup *m_admintools;      // session operator actions
	QActionGroup *m_modtools;        // session moderator tools
	QActionGroup *m_canvasbgtools;   // tools related to canvas background feature
	QActionGroup *m_resizetools;     // tools related to canvas resizing feature
	QActionGroup *m_putimagetools;   // Cut&Paste related tools
	QActionGroup *m_undotools;       // undo&redo related tools
	QActionGroup *m_drawingtools;    // drawing tool selection
	QActionGroup *m_brushSlots;      // tool slot shortcuts

	int m_lastToolBeforePaste; // Last selected tool before Paste was used

	QMetaObject::Connection m_textCopyConnection;

	// Remember window state to return from fullscreen mode
	QRect m_fullscreenOldGeometry;
	bool m_fullscreenOldMaximized;

	QElapsedTimer m_toolChangeTime; // how long the user has held down the tool change button
	ShortcutDetector *m_tempToolSwitchShortcut;

	Document *m_doc;
	bool m_exitAfterSave;
};

#endif

