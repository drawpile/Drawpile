// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_CANVASVIEW
#define DESKTOP_SCENE_CANVASVIEW
#include "desktop/scene/toggleitem.h"
#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/canvasshortcuts.h"
#include "libclient/canvas/point.h"
#include "libclient/tools/tool.h"
#include "libclient/utils/kis_cubic_curve.h"
#include <QGraphicsView>
#include <functional>

class QGestureEvent;
class QTouchEvent;

namespace drawingboard {
class CanvasScene;
}

namespace widgets {

class NotificationBar;

/**
 * @brief Editor view
 *
 * The editor view is a customized QGraphicsView that displays
 * the drawing board and handes user input.
 * It also provides other features, such as brush outline preview.
 */
class CanvasView final : public QGraphicsView {
	Q_OBJECT
public:
	enum class BrushCursor : int {
		Dot,
		Cross,
		Arrow,
		TriangleRight,
		TriangleLeft,
		Eraser,
		SameAsBrush = -1,
	};
	Q_ENUM(BrushCursor)

	enum class Direction { Left, Right, Up, Down };

	enum class Lock : unsigned int {
		None = 0,
		Reset = 1 << 0,
		Canvas = 1 << 1,
		User = 1 << 2,
		LayerLocked = 1 << 3,
		LayerGroup = 1 << 4,
		LayerCensored = 1 << 5,
		LayerHidden = 1 << 6,
		LayerHiddenInFrame = 1 << 7,
		Tool = 1 << 8,
	};
	Q_ENUM(Lock)

	CanvasView(QWidget *parent = nullptr);

	//! Set the board to use
	void setCanvas(drawingboard::CanvasScene *scene);

	//! Get the current zoom factor
	qreal zoom() const { return m_zoom; }
	qreal actualZoom() const { return actualZoomFor(m_zoom); }
	qreal actualZoomFor(qreal zoom) const { return zoom / devicePixelRatioF(); }

	//! Get the current rotation angle in degrees
	qreal rotation() const
	{
		qreal rotate = isRotationInverted() ? 360.0 - m_rotate : m_rotate;
		return rotate > 180.0 ? rotate - 360.0 : rotate;
	}

	bool isTabletEnabled() const { return m_enableTablet; }
	bool isTouchScrollEnabled() const { return m_enableTouchScroll; }
	bool isTouchDrawEnabled() const { return m_enableTouchDraw; }

	canvas::Point mapToCanvas(
		long long timeMsec, const QPoint &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation) const;

	canvas::Point mapToCanvas(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation) const;

	QPointF mapToCanvas(const QPoint &point) const;
	QPointF mapToCanvas(const QPointF &point) const;
	QPolygonF mapToCanvas(const QRect &rect) const;
	QPolygonF mapToCanvas(const QRectF &rect) const;

	QPointF mapFromCanvas(const QPointF &point) const;
	QPolygonF mapFromCanvas(const QRect &rect) const;

	//! The center point of the view in scene coordinates
	QPoint viewCenterPoint() const;

	//! Enable/disable tablet event handling
	void setTabletEnabled(bool enable) { m_enableTablet = enable; }

	//! Enable/disable touch gestures
	void setTouchScroll(bool scroll) { m_enableTouchScroll = scroll; }
	void setTouchDraw(bool draw) { m_enableTouchDraw = draw; }
	void setTouchPinch(bool pinch) { m_enableTouchPinch = pinch; }
	void setTouchTwist(bool twist) { m_enableTouchTwist = twist; }
	void setTouchUseGestureEvents(bool touchUseGestureEvents);
	void setRenderSmooth(bool smooth);
	void setRenderUpdateFull(bool updateFull);

	KisCubicCurve pressureCurve() const { return m_pressureCurve; }
	void setPressureCurve(const KisCubicCurve &pressureCurve);

	//! Is drawing in progress at the moment?
	bool isPenDown() const { return m_pendown != NOTDOWN; }

	//! Is this point (scene coordinates) inside the viewport?
	bool isPointVisible(const QPointF &point) const;

	//! Scroll view by the given number of pixels
	void scrollBy(int x, int y);

	//! Show the notification bar with the "reconnect" button visible
	void showDisconnectedWarning(const QString &message);
	void hideDisconnectedWarning();
	void showResetNotice(bool compatibilityMode, bool saveInProgress);
	void hideResetNotice();

	QString lockDescription() const;

signals:
	//! An image has been dropped on the widget
	void imageDropped(const QImage &image);

	//! An URL was dropped on the widget
	void urlDropped(const QUrl &url);

	//! A color was dropped on the widget
	void colorDropped(const QColor &color);

	//! The view has been transformed
	void viewTransformed(qreal zoom, qreal angle);

	//! Pointer moved in pointer tracking mode
	void pointerMoved(const QPointF &point);

	void penDown(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool right, float zoom,
		bool eraserOverride);
	void penMove(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool shift, bool alt);
	void penHover(const QPointF &point);
	void penUp();
	void quickAdjust(qreal value);
	void coordinatesChanged(const QPointF &coordinates);

	void viewRectChange(const QPolygonF &viewport);

	void rightClicked(const QPoint &p);

	void reconnectRequested();
	void savePreResetStateRequested();
	void savePreResetStateDismissed();

	void toggleActionActivated(drawingboard::ToggleItem::Action action);

public slots:
	//! Set the size of the brush preview outline
	void setOutlineSize(int size);

	//! Set subpixel precision mode and shape for brush preview outline
	void setOutlineMode(bool subpixel, bool square, bool force);

	//! Enable or disable pixel grid (shown only at high zoom levels)
	void setPixelGrid(bool enable);

	//! Scroll view to location
	void scrollTo(const QPointF &point);

	//! Set the zoom factor in percents, centered on the middle of the view
	void setZoom(qreal zoom);

	//! Set the zoom factor in percents, centered on the given point
	void setZoomAt(qreal zoom, const QPointF &point);

	//! Set the rotation angle in degrees
	void setRotation(qreal angle);

	void setViewFlip(bool flip);
	void setViewMirror(bool mirror);

	void setLock(QFlags<Lock> lock);
	void setBusy(bool busy);
	void setSaveInProgress(bool saveInProgress);

	//! Send pointer position updates even when not drawing
	void setPointerTracking(bool tracking);

	void moveStep(Direction direction);

	//! Increase/decrease zoom factor by this many steps
	void zoomSteps(int steps);
	void zoomStepsAt(int steps, const QPointF &point);

	//! Increase zoom factor
	void zoomin();

	//! Decrease zoom factor
	void zoomout();

	//! Zoom the view it's filled by the given rectangle
	//! If the rectangle is very small, or steps are negative, just zoom by that
	//! many steps
	void zoomTo(const QRect &rect, int steps);

	//! Zoom to fit the view
	void zoomToFit();
	void zoomToFitWidth();
	void zoomToFitHeight();

	void setToolCursor(const QCursor &cursor);
	void setToolCapabilities(
		bool allowColorPick, bool allowToolAdjust, bool toolHandlesRightClick);

	void setBrushCursorStyle(BrushCursor style);
	void setEraseCursorStyle(BrushCursor style);
	void setAlphaLockCursorStyle(BrushCursor style);
	void setBrushOutlineWidth(qreal outlineWidth);
	void setBrushBlendMode(int brushBlendMode);

	void setSuppressTransformNotices(bool suppressTransformNotices);
#ifdef __EMSCRIPTEN__
	void setEnableEraserOverride(bool enableEraserOverride);
#endif

protected:
	void enterEvent(compat::EnterEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void focusInEvent(QFocusEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent *) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	bool viewportEvent(QEvent *event) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *) override;
	void scrollContentsBy(int dx, int dy) override;

private slots:
	void activateNotificationBarAction();
	void dismissNotificationBar();

private:
	static constexpr qreal TOUCH_DRAW_DISTANCE = 10.0;
	static constexpr int TOUCH_DRAW_BUFFER_COUNT = 20;
	static constexpr qreal ROTATION_STEP_SIZE = 15.0;

	enum class NotificationBarState { None, Reconnect, Reset };

	// unified mouse/stylus event handlers
	void penPressEvent(
		QEvent *event, long long timeMsec, const QPointF &pos, qreal pressure,
		qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers, bool isStylus, bool eraserOverride);
	void penMoveEvent(
		long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, Qt::MouseButtons buttons,
		Qt::KeyboardModifiers modifiers);
	void penReleaseEvent(
		long long timeMsec, const QPointF &pos, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers);

	void touchPressEvent(QEvent *event, long long timeMsec, const QPointF &pos);
	void touchMoveEvent(long long timeMsec, const QPointF &pos);
	void touchReleaseEvent(long long timeMsec, const QPointF &pos);
	void gestureEvent(QGestureEvent *event);

	enum class ViewDragMode { None, Prepared, Started };

	//! Drag the view
	void moveDrag(const QPoint &point);

	QTransform fromCanvasTransform() const;
	QTransform toCanvasTransform() const;

	void updateCanvasTransform(const std::function<void()> &block);
	void updatePosBounds();
	void clampPosition();
	void updateScrollBars();
	void updateCanvasPixelGrid();
	void updateRenderHints();

	QTransform calculateCanvasTransform() const;

	QTransform calculateCanvasTransformFrom(
		const QPointF &pos, qreal zoom, qreal rotate, bool mirror,
		bool flip) const;

	static void mirrorFlip(QTransform &matrix, bool mirror, bool flip);

	void emitViewTransformed();
	bool isRotationInverted() const;

	void updateOutlinePos(QPointF point);
	void updateOutline();
	QPointF getOutlinePos() const;
	qreal getOutlineWidth() const;
	BrushCursor getCurrentCursorStyle() const;

#ifdef HAVE_EMULATED_BITMAP_CURSOR
	void updateCursorPos(const QPoint &pos);
#endif

	void onPenDown(const canvas::Point &p, bool right, bool eraserOverride);
	void onPenMove(
		const canvas::Point &p, bool right, bool constrain1, bool constrain2);
	void onPenUp();

	void flushTouchDrawBuffer();

	void touchEvent(QTouchEvent *event);

	void resetCursor();
	void setViewportCursor(const QCursor &cursor);

	void setZoomToFit(Qt::Orientations orientations);
	void rotateByDiscreteSteps(int steps);

	void viewRectChanged();

	QString getZoomNotice() const;
	QString getRotationNotice() const;
	void showTouchTransformNotice();
	void showTransformNotice(const QString &text);
	void updateLockNotice();

	Qt::KeyboardModifiers getTabletModifiers(const QTabletEvent *tabev) const;

	CanvasShortcuts m_canvasShortcuts;
	QSet<Qt::Key> m_keysDown;

	/**
	 * @brief State of the pen
	 *
	 * - NOTDOWN pen is not down
	 * - MOUSEDOWN mouse is down
	 * - TABLETDOWN tablet stylus is down
	 */
	enum { NOTDOWN, MOUSEDOWN, TABLETDOWN } m_pendown;

	enum class PenMode { Normal, Colorpick, Layerpick };

	enum class TouchMode { Unknown, Drawing, Moving };

	NotificationBar *m_notificationBar;
	NotificationBarState m_notificationBarState = NotificationBarState::None;

	bool m_allowColorPick;
	bool m_allowToolAdjust;
	bool m_toolHandlesRightClick;
	PenMode m_penmode;

	//! Is the view being dragged
	ViewDragMode m_dragmode;
	CanvasShortcuts::Action m_dragAction;
	Qt::MouseButton m_dragButton;
	Qt::KeyboardModifiers m_dragModifiers;
	bool m_dragInverted;
	bool m_dragSwapAxes;
	QPoint m_dragLastPoint;
	QPointF m_dragCanvasPoint;
	qreal m_dragDiscreteRotation;

	//! Previous pointer location
	canvas::Point m_prevpoint;
	QPointF m_prevoutlinepoint;
	qreal m_pointerdistance;
	qreal m_pointervelocity;

	qreal m_gestureStartZoom;
	qreal m_gestureStartAngle;

	int m_outlineSize;
	bool m_showoutline, m_subpixeloutline, m_forceoutline;
	QCursor m_dotcursor, m_trianglerightcursor, m_triangleleftcursor;
	QCursor m_colorpickcursor, m_layerpickcursor, m_zoomcursor, m_rotatecursor;
	QCursor m_rotatediscretecursor, m_erasercursor, m_toolcursor;

	QPointF m_pos;		// Canvas position
	qreal m_zoom;		// View zoom in percents
	qreal m_rotate;		// View rotation in degrees
	bool m_flip;		// Flip Y axis
	bool m_mirror;		// Flip X axis
	QRectF m_posBounds; // Position limits to keep the canvas in view.

	drawingboard::CanvasScene *m_scene;

	int m_zoomWheelDelta;

	bool m_enableTablet;
	QFlags<Lock> m_lock;
	bool m_busy;
	bool m_pointertracking;
	bool m_pixelgrid;

	bool m_enableTouchScroll, m_enableTouchDraw;
	bool m_enableTouchPinch, m_enableTouchTwist;
	bool m_useGestureEvents;
	bool m_touching, m_touchRotating;
	TouchMode m_touchMode;
	QVector<QPair<long long, QPointF>> m_touchDrawBuffer;
	qreal m_touchStartZoom, m_touchStartRotate;

	KisCubicCurve m_pressureCurve;

	qreal m_dpi;
	BrushCursor m_brushCursorStyle;
	BrushCursor m_eraseCursorStyle;
	BrushCursor m_alphaLockCursorStyle;
	qreal m_brushOutlineWidth;
	int m_brushBlendMode;

	bool m_scrollBarsAdjusting;
	bool m_blockNotices;
	bool m_suppressTransformNotices;
	bool m_hoveringOverHud;
	bool m_renderSmooth;
#ifdef __EMSCRIPTEN__
	bool m_enableEraserOverride = false;
#endif
};

}

#endif
