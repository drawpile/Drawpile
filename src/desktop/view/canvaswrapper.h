// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASWRAPPER_H
#define DESKTOP_VIEW_CANVASWRAPPER_H
#include <QPoint>
#include <QRectF>
#include <QString>

class Document;
class MainWindow;
class QAction;
class QAbstractScrollArea;
class QWidget;

namespace canvas {
class CanvasModel;
}

namespace dialogs {
class LoginDialog;
}

namespace docks {
class Navigator;
class ToolSettings;
}

namespace drawingboard {
class AnnotationItem;
}

namespace tools {
class ToolController;
}

namespace widgets {
class CanvasFrame;
class ViewStatus;
class ViewStatusBar;
}

namespace view {

class Lock;

// Wraps the old drawingboard::CanvasView and the new view::CanvasView to be
// able to switch between them. This is supposed to be a tempoarry measure to
// allow the new implementation to stabilize before switching over to it.
class CanvasWrapper {
public:
	struct Actions {
		QAction *moveleft;
		QAction *moveright;
		QAction *moveup;
		QAction *movedown;
		QAction *zoomin;
		QAction *zoomincenter;
		QAction *zoomout;
		QAction *zoomoutcenter;
		QAction *zoomorig;
		QAction *zoomorigcenter;
		QAction *zoomfit;
		QAction *zoomfitwidth;
		QAction *zoomfitheight;
		QAction *rotateorig;
		QAction *rotatecw;
		QAction *rotateccw;
		QAction *viewflip;
		QAction *viewmirror;
		QAction *showgrid;
		QAction *showusermarkers;
		QAction *showusernames;
		QAction *showuserlayers;
		QAction *showuseravatars;
		QAction *evadeusercursors;
	};

	static CanvasWrapper *
	instantiate(int canvasImplementation, QWidget *parent);

	virtual ~CanvasWrapper();

	virtual QAbstractScrollArea *viewWidget() const = 0;

	virtual bool isTabletEnabled() const = 0;
	virtual bool isTouchScrollEnabled() const = 0;
	virtual bool isTouchDrawEnabled() const = 0;
	virtual QString pressureCurveAsString() const = 0;

	virtual QPoint viewCenterPoint() const = 0;
	virtual QPointF viewTransformOffset() const = 0;
	virtual bool isPointVisible(const QPointF &point) const = 0;
	virtual QRectF screenRect() const = 0;

	virtual canvas::CanvasModel *canvas() const = 0;
	virtual void setCanvas(canvas::CanvasModel *canvas) = 0;

	virtual drawingboard::AnnotationItem *
	getAnnotationItem(int annotationId) = 0;

	virtual void clearKeys() = 0;

#ifdef __EMSCRIPTEN__
	virtual void setEnableEraserOverride(bool enableEraserOverride) = 0;
#endif

	virtual void setShowAnnotations(bool showAnnotations) = 0;
	virtual void setShowAnnotationBorders(bool showAnnotationBorders) = 0;
	virtual void setShowLaserTrails(bool showLaserTrails) = 0;
	virtual void setShowOwnUserMarker(bool showOwnUserMarker) = 0;
	virtual void setPointerTracking(bool pointerTracking) = 0;
	virtual void setShowToggleItems(bool showToggleItems) = 0;

	virtual void setCatchupProgress(int percent, bool force) = 0;
	virtual void setStreamResetProgress(int percent) = 0;
	virtual void setSaveInProgress(bool saveInProgress) = 0;

	virtual void
	showDisconnectedWarning(const QString &message, bool singleSession) = 0;
	virtual void hideDisconnectedWarning() = 0;
	virtual void showResetNotice(bool saveInProgress) = 0;
	virtual void hideResetNotice() = 0;

	virtual void showPopupNotice(const QString &message) = 0;

	virtual void disposeScene() = 0;

	virtual void connectActions(const Actions &actions) = 0;

	virtual void connectCanvasFrame(widgets::CanvasFrame *canvasFrame) = 0;

	virtual void connectDocument(Document *doc) = 0;

	virtual void connectLock(Lock *lock) = 0;

	virtual void
	connectLoginDialog(Document *doc, dialogs::LoginDialog *dlg) = 0;

	virtual void connectMainWindow(MainWindow *mainWindow) = 0;

	virtual void connectNavigator(docks::Navigator *navigator) = 0;

	virtual void connectToolSettings(docks::ToolSettings *toolSettings) = 0;

	virtual void connectViewStatus(widgets::ViewStatus *viewStatus) = 0;

	virtual void
	connectViewStatusBar(widgets::ViewStatusBar *viewStatusBar) = 0;
};

}

#endif
