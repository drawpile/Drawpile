// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include "desktop/utils/qtguicompat.h"
#include "libclient/canvas/canvasshortcuts.h"
#include "libclient/canvas/point.h"
#include "libclient/tools/tool.h"
#include "libclient/utils/kis_cubic_curve.h"

#include <QGraphicsView>
#include <functional>

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
		TriangleLeft
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
		Tool = 1 << 7,
	};
	Q_ENUM(Lock)

	CanvasView(QWidget *parent = nullptr);

	//! Set the board to use
	void setCanvas(drawingboard::CanvasScene *scene);

	//! Get the current zoom factor
	qreal zoom() const { return m_zoom; }

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
		qreal ytilt, qreal rotation, bool right, float zoom);
	void penMove(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool shift, bool alt);
	void penHover(const QPointF &point);
	void penUp();
	void quickAdjust(qreal value);

	void viewRectChange(const QPolygonF &viewport);

	void rightClicked(const QPoint &p);

	void reconnectRequested();

public slots:
	//! Set the size of the brush preview outline
	void setOutlineSize(int size);

	//! Set subpixel precision mode and shape for brush preview outline
	void setOutlineMode(bool subpixel, bool square);

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

	//! Send pointer position updates even when not drawing
	void setPointerTracking(bool tracking);

	void moveStep(Direction direction);

	//! Increase/decrease zoom factor by this many steps
	void zoomSteps(int steps, const QPointF &point);

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

	void setBrushCursorStyle(BrushCursor style) { m_brushCursorStyle = style; }
	void setBrushOutlineWidth(qreal outlineWidth);

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
	void drawForeground(QPainter *painter, const QRectF &rect) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void resizeEvent(QResizeEvent *) override;
	void scrollContentsBy(int dx, int dy) override;

private:
	static constexpr qreal TOUCH_DRAW_DISTANCE = 10.0;
	static constexpr int TOUCH_DRAW_BUFFER_COUNT = 20;

	// unified mouse/stylus event handlers
	void penPressEvent(
		long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers, bool isStylus);
	void penMoveEvent(
		long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, Qt::MouseButtons buttons,
		Qt::KeyboardModifiers modifiers);
	void penReleaseEvent(
		long long timeMsec, const QPointF &pos, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers);

	void touchPressEvent(long long timeMsec, const QPointF &pos);
	void touchMoveEvent(long long timeMsec, const QPointF &pos);
	void touchReleaseEvent(long long timeMsec, const QPointF &pos);

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

	QTransform calculateCanvasTransform() const;

	static QTransform calculateCanvasTransformFrom(
		const QPointF &pos, qreal zoom, qreal rotate, bool mirror, bool flip);

	static void mirrorFlip(QTransform &matrix, bool mirror, bool flip);

	void emitViewTransformed();
	bool isRotationInverted() const;

	//! Redraw the scene around the outline cursor if necesasry
	void updateOutline(canvas::Point point);
	void updateOutline();
	QRectF getOutlineBounds(const QPointF &point, int size);

	void onPenDown(const canvas::Point &p, bool right);
	void onPenMove(
		const canvas::Point &p, bool right, bool constrain1, bool constrain2);
	void onPenUp();

	void flushTouchDrawBuffer();

	void touchEvent(QTouchEvent *event);

	void resetCursor();

	void setZoomToFit(Qt::Orientations orientations);

	void viewRectChanged();

	QString getZoomNotice() const;
	QString getRotationNotice() const;
	void showTransformNotice(const QString &text);
	void updateLockNotice();

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

	bool m_allowColorPick;
	bool m_allowToolAdjust;
	bool m_toolHandlesRightClick;
	PenMode m_penmode;

	//! Is the view being dragged
	ViewDragMode m_dragmode;
	CanvasShortcuts::Action m_dragAction;
	bool m_dragByKey;
	bool m_dragInverted;
	bool m_dragSwapAxes;
	QPoint m_dragLastPoint;

	//! Previous pointer location
	canvas::Point m_prevpoint;
	canvas::Point m_prevoutlinepoint;
	bool m_prevoutline;
	qreal m_pointerdistance;
	qreal m_pointervelocity;

	int m_outlineSize;
	bool m_showoutline, m_subpixeloutline, m_squareoutline;
	QCursor m_dotcursor, m_trianglerightcursor, m_triangleleftcursor;
	QCursor m_colorpickcursor, m_layerpickcursor, m_zoomcursor, m_rotatecursor;
	QCursor m_toolcursor;

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
	bool m_touching, m_touchRotating;
	TouchMode m_touchMode;
	QVector<QPair<long long, QPointF>> m_touchDrawBuffer;
	qreal m_touchStartZoom, m_touchStartRotate;

	KisCubicCurve m_pressureCurve;

	qreal m_dpi;
	BrushCursor m_brushCursorStyle;
	qreal m_brushOutlineWidth;

	bool m_scrollBarsAdjusting;
	bool m_blockNotices;
};

}

#endif
