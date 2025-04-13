// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_MAINWINDOW_H
#define DESKTOP_MAINWINDOW_H
extern "C" {
#include <dpengine/load_enums.h>
}
#include "desktop/dialogs/flipbook.h"
#include "desktop/utils/hostparams.h"
#include "libclient/canvas/acl.h"
#include "libclient/drawdance/canvasstate.h"
#include "libclient/tools/tool.h"
#include <QByteArray>
#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QMainWindow>
#include <QMap>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantMap>
#include <functional>

class ActionBuilder;
class Document;
class MainActions;
class QActionGroup;
class QLabel;
class QShortcutEvent;
class QSplitter;
class QTemporaryFile;
class QToolButton;

namespace widgets {
class CanvasFrame;
class DualColorButton;
class NetStatus;
class ChatBox;
class ViewStatus;
class ViewStatusBar;
}

namespace docks {
class ToolSettings;
class BrushPalette;
class InputSettings;
class LayerList;
class PaletteBox;
class ColorCircleDock;
class ColorPaletteDock;
class ColorSpinnerDock;
class ColorSliderDock;
class Navigator;
class OnionSkinsDock;
class ReferenceDock;
class Timeline;
}

namespace dialogs {
class DumpPlaybackDialog;
class PlaybackDialog;
class HostDialog;
class SessionSettingsDialog;
class ServerLogDialog;
class SettingsDialog;
class StartDialog;
class TabletTestDialog;
class TouchTestDialog;
}

namespace canvas {
class CanvasModel;
class SelectionModel;
}

namespace drawdance {
class CanvasState;
}

namespace desktop {
namespace settings {
class Settings;
}
}

namespace view {
class CanvasWrapper;
class Lock;
}

class ShortcutDetector;

//! The application main window
class MainWindow final : public QMainWindow {
	Q_OBJECT
public:
	MainWindow(bool restoreWindowPosition = true, bool singleSession = false);
	~MainWindow() override;

	void openRecent(const QString &path, QTemporaryFile *tempFile = nullptr);
	void openPath(const QString &path, QTemporaryFile *tempFile = nullptr);
	void autoJoin(const QUrl &url, const QString &autoRecordPath);

	void hostSession(const HostParams &params);

	//! Connect to a host and join a session if full URL is provided.
	void
	joinSession(const QUrl &url, const QString &autoRecordFilename = QString());

	//! Check if the current board can be replaced
	bool canReplace() const;

#ifdef __EMSCRIPTEN__
	bool shouldPreventUnload() const;
	void handleMouseLeave();
#endif

#ifndef __EMSCRIPTEN__
	//! Save settings and exit
	void exit();
#endif

	dialogs::StartDialog *showStartDialog();

	void showPopupMessage(const QString &message);
	void showPermissionDeniedMessage(int feature);

	bool notificationsMuted() const { return m_notificationsMuted; }
	bool isInitialCatchup() const { return m_initialCatchup; }

signals:
	void hostSessionEnabled(bool enabled);
	void smallScreenModeChanged(bool smallScreenMode);
	void windowReplacementFailed(MainWindow *win);
	void viewShifted(qreal deltaX, qreal deltaY);
	void dockTabUpdateRequested();
	void intendedDockStateRestoreRequested();
	void resizeReactionRequested();
	void selectionMaskVisibilityChanged(bool visible);

public slots:
	// Triggerable actions
	void start();
	void showNew();
	void open();
#ifdef __EMSCRIPTEN__
	void download();
	void downloadSelection();
#else
	bool save();
	void saveas();
	void saveSelection();
	void exportImage();
#	ifndef Q_OS_ANDROID
	void exportImageAgain();
#	endif
#endif
	void importAnimationFrames();
	void importAnimationLayers();
	void showFlipbook();

	dialogs::SettingsDialog *showSettings();
	dialogs::TabletTestDialog *showTabletTestDialog(QWidget *parent);
	dialogs::TouchTestDialog *showTouchTestDialog(QWidget *parent);
	void reportAbuse();
	void tryToGainOp();
	void resetSession();
	void terminateSession();

	void host();
	void invite();
	void join();
	void reconnect();
	void browse();
	void leave();
#ifndef __EMSCRIPTEN__
	void checkForUpdates();
#endif

	void toggleFullscreen();
	void setShowAnnotations(bool show);
	void setShowLaserTrails(bool show);

	void selectTool(QAction *tool);

	static void about();
	static void homepage();

	//! Create a blank new document
	void newDocument(const QSize &size, const QColor &background);

	void dropImage(const QImage &image);
	void dropUrl(const QUrl &url);
	void handleToggleAction(int action);
	void handleTouchTapAction(int action);

	void savePreResetImageAs();
	void discardPreResetImage();

private slots:
	void showSystemInfo();
	void toggleRecording();
	void toggleProfile();
	void toggleTabletEventLog();

	void exportTemplate();
	void updateFlipbookState();

	void showResetNoticeDialog(const drawdance::CanvasState &canvasState);
	void updateCatchupProgress(int percent);
	void updateStreamResetProgress(int percent);

	void onOperatorModeChange(bool op);
	void onFeatureAccessChange(DP_Feature feature, bool canUse);
	void onBrushSizeLimitChange(int brushSizeLimit);
	void onUndoDepthLimitSet(int undoDepthLimit);

	void onServerConnected();
	void onServerLogin(bool join, const QString &joinPassword);
	void onServerDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
	void onNsfmChanged(bool nsfm);

	void updateLockWidget();
	void setRecorderStatus(bool on);

	void loadShortcuts(const QVariantMap &shortcuts);
	void setBrushSlotCount(int count);

	void toggleLayerViewMode();
	void updateLayerViewMode();

	void copyText();
	void paste();
	void pasteCentered();
	void pasteFile();
	void pasteFilePath(const QString &path);
	void pasteImage(
		const QImage &image, const QPoint *point = nullptr, bool force = false);

	void clearOrDelete();

	void resizeCanvas(int expandDirection);
	void updateBackgroundActions();
	void changeCanvasBackground();
	void changeLocalCanvasBackground();
	void clearLocalCanvasBackground();

	void showLayoutsDialog();
	void showUserInfoDialog(int userId);
	void showBrushSettingsDialogBrush();
	void showBrushSettingsDialogPreset();

	void showAlterSelectionDialog();
	void alterSelection(int expand, int kernel, int feather, bool fromEdge);
	void changeUndoDepthLimit();

	void updateDevToolsActions();
	void setArtificialLag();
	void setArtificialDisconnect();
#ifndef __EMSCRIPTEN__
	void toggleDebugDump();
#endif
	void openDebugDump();
	void causeCrash();

	void updateTemporaryToolSwitch();
	void toolChanged(tools::Tool::Type tool);
	void updateFreehandToolButton(int brushMode);
	void handleFreehandToolButtonClicked();

	void updateSelectTransformActions();
	void updateSelectionMaskVisibility();

	void setFreezeDocks(bool freeze);
	void setDocksHidden(bool hidden);
	void setDockArrangeMode(bool arrange);
	QAction *finishArrangingDocks();
	void updateSideTabDocks();
	void setNotificationsMuted(bool muted);
	void setToolState(int toolState);

	void updateTitle();

	void onCanvasChanged(canvas::CanvasModel *canvas);
	void onCanvasSaveStarted();
	void onCanvasSaved(const QString &errorMessage);
#ifdef __EMSCRIPTEN__
	void onCanvasDownloadStarted();
	void
	onCanvasDownloadReady(const QString &defaultName, const QByteArray &bytes);
	void onCanvasDownloadError(const QString &errorMessage);
	void offerDownload(const QString &defaultName, const QByteArray &bytes);
#endif
	void onTemplateExported(const QString &errorMessage);

	bool eventFilter(QObject *object, QEvent *event) override;

protected:
	void resizeEvent(QResizeEvent *event) override;
	void closeEvent(QCloseEvent *event) override;
	bool event(QEvent *event) override;

private:
	static constexpr int DESKTOP_MODE_MIN_WIDTH = 1200;
	static constexpr int DESKTOP_MODE_MIN_HEIGHT = 700;
	enum class ReplacementCriterion {
		Dirty = (1 << 0),
		Connected = (1 << 1),
		Recording = (1 << 2),
		Playback = (1 << 3),
	};
	Q_DECLARE_FLAGS(ReplacementCriteria, ReplacementCriterion)

	ReplacementCriteria getReplacementCriteria() const;
	void
	questionOpenFileWindowReplacement(const std::function<void(bool)> &block);
	void questionWindowReplacement(
		const QString &title, const QString &action,
		const std::function<void(bool)> &block);
	void prepareWindowReplacement();
	void createNewWindow(const std::function<void(MainWindow *)> &block);

	void connectStartDialog(dialogs::StartDialog *dlg);
	void setStartDialogActions(dialogs::StartDialog *dlg);
	void closeStartDialog(dialogs::StartDialog *dlg, bool join);
	QWidget *getStartDialogOrThis();

	void showBrushSettingsDialog(bool openOnPresetPage);

	void connectToSession(const QUrl &url, const QString &autoRecordFilename);
	void importAnimation(int source);
	void showAnimationExportDialog(bool fromFlipbook);
	void exportAnimation(
#ifndef __EMSCRIPTEN__
		const QString &path,
#endif
		int format, int loops, int start, int end, int framerate,
		const QRect &crop, int scalePercent, bool scaleSmooth);

	ActionBuilder makeAction(const char *name, const QString &text = QString{});
	QAction *getAction(const QString &name);
	QAction *searchAction(const QString &name);

	void addBrushShortcut(
		const QString &name, const QString &text, const QKeySequence &shortcut);
	void changeBrushShortcut(const QString &name, const QString &text);
	void removeBrushShortcut(const QString &name);
	void triggerBrushShortcut(QAction *action);

	//! Add a new entry to recent files list
	void addRecentFile(const QString &file);

	//! Enable or disable drawing tools
	void setDrawingToolsEnabled(bool enable);

	void aboutToShowMenu();
	void aboutToHideMenu();
#if !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
	void updateExportPath(const QString &path);
#endif

	//! Display an error message
	void showErrorMessage(const QString &message);
	void
	showErrorMessageWithDetails(const QString &message, const QString &details);
	void showLoadResultMessage(DP_LoadResult result);
	void handleAmbiguousShortcut(QShortcutEvent *shortcutEvent);

	void showSelectionMaskColorPicker();

	void readSettings(bool windowpos = true);
	void restoreSettings(const desktop::settings::Settings &settings);
	void initSmallScreenState();
	void initDefaultDocks();
	void setDefaultDockSizes();
	void saveSplitterState();
	void saveWindowState();

	void requestUserInfo(int userId);
	void sendUserInfo(int userId);
	void requestCurrentBrush(int userId);
	void sendCurrentBrush(int userId, const QString &correlator);
	void receiveCurrentBrush(int userId, const QJsonObject &info);

	void fillArea(const QColor &color, int blendMode, float opacity);
	void fillAreaWithBlendMode(int blendMode);

	void createDocks();
	void resetDefaultDocks();
	void resetDefaultToolbars();
	void setupActions();
	void setupBrushShortcuts();
	void updateInterfaceModeActions();
	void reenableUpdates();
	void keepCanvasPosition(const std::function<void()> &block);
	void reactToResize();

	static bool isInitialSmallScreenMode();
	void updateInterfaceMode();
	bool shouldUseSmallScreenMode(const desktop::settings::Settings &settings);
	static bool isSmallScreenModeSize(const QSize &s);
	void switchInterfaceMode(bool smallScreenMode);
	bool shouldShowDialogMaximized() const;

	void startIntendedDockStateDebounce();
	void updateIntendedDockState();
	bool canRememberDockStateFromWindow() const;
	void restoreIntendedDockState();
	void startRefitWindowDebounce();
	void refitWindow();

	void deactivateAllDocks();
	void prepareDockTabUpdate();
	void updateDockTabs();

	bool m_singleSession;
	bool m_smallScreenMode;
	bool m_updatingInterfaceMode;
	QByteArray m_desktopModeState;
	QDeadlineTimer m_lastDisconnectNotificationTimer;

	QTimer m_saveWindowDebounce;
	QTimer m_saveSplitterDebounce;
	QTimer m_updateIntendedDockStateDebounce;
	QTimer m_restoreIntendedDockStateDebounce;
#ifdef SINGLE_MAIN_WINDOW
	QTimer m_refitWindowDebounce;
	bool m_refitting = false;
#endif
	QMap<QString, bool> m_actionsConfig;

	QSplitter *m_splitter;
	int m_splitterOriginalHandleWidth;

	docks::ToolSettings *m_dockToolSettings;
	docks::BrushPalette *m_dockBrushPalette;
	docks::InputSettings *m_dockInput;
	docks::LayerList *m_dockLayers;
	docks::ColorPaletteDock *m_dockColorPalette;
	docks::ColorSpinnerDock *m_dockColorSpinner;
	docks::ColorSliderDock *m_dockColorSliders;
	docks::ColorCircleDock *m_dockColorCircle;
	docks::Navigator *m_dockNavigator;
	docks::OnionSkinsDock *m_dockOnionSkins;
	docks::Timeline *m_dockTimeline;
	docks::ReferenceDock *m_dockReference;
	QToolBar *m_toolBarFile;
	QToolBar *m_toolBarEdit;
	QToolBar *m_toolBarDraw;
	QWidget *m_smallScreenSpacer;
	QAction *m_freehandAction;
	QToolButton *m_freehandButton;
	QByteArray m_intendedDockState;
	QByteArray m_hiddenDockState;
	widgets::ChatBox *m_chatbox;
	widgets::DualColorButton *m_dualColorButton;

	view::Lock *m_viewLock;
	widgets::CanvasFrame *m_canvasFrame;
	view::CanvasWrapper *m_canvasView;

	widgets::ViewStatusBar *m_viewStatusBar;
	QLabel *m_lockstatus;
	widgets::NetStatus *m_netstatus;
	widgets::ViewStatus *m_viewstatus;
	QToolButton *m_statusChatButton;

	QPointer<dialogs::PlaybackDialog> m_playbackDialog;
	QPointer<dialogs::DumpPlaybackDialog> m_dumpPlaybackDialog;
	dialogs::SessionSettingsDialog *m_sessionSettings;
	dialogs::ServerLogDialog *m_serverLogDialog;
	dialogs::Flipbook::State m_flipbookState;
	int m_animationExportScalePercent = 100;
	bool m_animationExportScaleSmooth = true;

#ifndef __EMSCRIPTEN__
	QMenu *m_recentMenu;
#endif
	QAction *m_lastLayerViewMode;

	QActionGroup
		*m_currentdoctools; // general tools that require no special permissions
	QActionGroup *m_admintools;	   // session operator actions
	QActionGroup *m_canvasbgtools; // tools related to canvas background feature
	QActionGroup *m_resizetools;   // tools related to canvas resizing feature
	QActionGroup *m_putimagetools; // Cut&Paste related tools
	QActionGroup *m_undotools;	   // undo&redo related tools
	QActionGroup *m_drawingtools;  // drawing tool selection
	QActionGroup *m_brushSlots;	   // tool slot shortcuts
	QActionGroup *m_dockToggles;   // dock visibility toggles
	QActionGroup *m_desktopModeActions;
	QActionGroup *m_smallScreenModeActions;
	QVector<QAction *> m_smallScreenEditActions;

	QMetaObject::Connection m_textCopyConnection;

#ifndef Q_OS_ANDROID
	// Remember window state to return from fullscreen mode
	QRect m_fullscreenOldGeometry;
	bool m_fullscreenOldMaximized;
#endif

	int m_temporaryToolSwitchMs = -1;
	QElapsedTimer m_toolChangeTime; // how long the user has held down the tool
									// change button
	ShortcutDetector *m_tempToolSwitchShortcut;
	bool m_wasSessionLocked;
	bool m_notificationsMuted;
	bool m_initialCatchup;
	bool m_toolStateNormal;
	bool m_dockTabUpdatePending = false;
	bool m_updatingDockState = false;
	bool m_resizeReactionPending = false;

	Document *m_doc;
	MainActions *m_ma;
#ifndef __EMSCRIPTEN__
	enum { RUNNING, DISCONNECTING, SAVING } m_exitAction;
#endif

	drawdance::CanvasState m_preResetCanvasState;

	int m_brushRequestUserId = -1;
	QString m_brushRequestCorrelator;
	QElapsedTimer m_brushRequestTime;
};

#endif
