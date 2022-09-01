/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#ifndef EDITORVIEW_H
#define EDITORVIEW_H

#include "canvasviewmodifiers.h"
#include "canvas/pressure.h"
#include "canvas/point.h"
#include "tools/tool.h"

#include <QGraphicsView>

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
class CanvasView : public QGraphicsView
{
	Q_OBJECT
public:
	CanvasView(QWidget *parent=nullptr);

	//! Set the board to use
	void setCanvas(drawingboard::CanvasScene *scene);

	//! Get the current zoom factor
	qreal zoom() const { return m_zoom; }

	//! Get the current rotation angle in degrees
	qreal rotation() const { return m_rotate; }

	using QGraphicsView::mapToScene;
	canvas::Point mapToScene(const QPoint &point, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation) const;
	canvas::Point mapToScene(const QPointF &point, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation) const;

	//! The center point of the view in scene coordinates
	QPoint viewCenterPoint() const;

	//! Enable/disable tablet event handling
	void setTabletEnabled(bool enable) { m_enableTablet = enable; }

	//! Enable/disable touch gestures
	void setTouchGestures(bool scroll, bool pinch, bool twist);

	//! Is drawing in progress at the moment?
	bool isPenDown() const { return m_pendown != NOTDOWN; }

	//! Is this point (scene coordinates) inside the viewport?
	bool isPointVisible(const QPointF &point) const;

	//! Scroll view by the given number of pixels
	void scrollBy(int x, int y);

	//! Get the scale factor needed to fit the whole canvas in the viewport
	qreal fitToWindowScale() const;

	//! Show the notification bar with the "reconnect" button visible
	void showDisconnectedWarning(const QString &message);

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

	void penDown(const QPointF &point, qreal pressure, bool right, float zoom);
	void penMove(const QPointF &point, qreal pressure, bool shift, bool alt);
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
	void scrollTo(const QPoint& point);

	//! Set the zoom factor in percents
	void setZoom(qreal zoom);

	//! Set the rotation angle in degrees
	void setRotation(qreal angle);

	void setViewFlip(bool flip);
	void setViewMirror(bool mirror);

	void setLocked(bool lock);

	//! Send pointer position updates even when not drawing
	void setPointerTracking(bool tracking);

	void setPressureMapping(const PressureMapping &mapping);

	//! Increase/decrease zoom factor by this many steps
	void zoomSteps(int steps);

	//! Increase zoom factor
	void zoomin();

	//! Decrease zoom factor
	void zoomout();

	//! Zoom the view it's filled by the given rectangle
	//! If the rectangle is very small, or steps are negative, just zoom by that many steps
	void zoomTo(const QRect &rect, int steps);

	//! Zoom to fit the view
	void zoomToFit();

	void setToolCursor(const QCursor &cursor);

	/**
	 * @brief Set the cursor to use for brush tools
	 * Styles:
	 * 0. Dot
	 * 1. Crosshair
	 * 2. Arrow
	 */
	void setBrushCursorStyle(int style, qreal outlineWidth);

	void updateShortcuts();

	void setEnableViewportEntryHack(bool enabled);

protected:
	void enterEvent(QEvent *event) override;
	void leaveEvent(QEvent *event) override;
	void mouseMoveEvent(QMouseEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void mouseReleaseEvent(QMouseEvent *event) override;
	void mouseDoubleClickEvent(QMouseEvent*) override;
	void wheelEvent(QWheelEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void keyReleaseEvent(QKeyEvent *event) override;
	bool viewportEvent(QEvent *event) override;
	void drawForeground(QPainter *painter, const QRectF& rect) override;
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dragMoveEvent(QDragMoveEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	void showEvent(QShowEvent *event) override;
	void scrollContentsBy(int dx, int dy) override;
	void resizeEvent(QResizeEvent *) override;

private:
	// unified mouse/stylus event handlers
	void penPressEvent(const QPointF &pos, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus);
	void penMoveEvent(const QPointF &pos, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool isStylus);
	void penReleaseEvent(const QPointF &pos, Qt::MouseButton button);

private:
	qreal mapPressure(qreal pressure, bool stylus);

	enum class ViewDragMode {None, Prepared, Started};

	//! Drag the view
	void moveDrag(const QPoint &point, Qt::KeyboardModifiers modifiers);

	//! Redraw the scene around the outline cursor if necesasry
	void updateOutline(canvas::Point point);
	void updateOutline();

	void onPenDown(const canvas::Point &p, bool right);
	void onPenMove(const canvas::Point &p, bool right, bool constrain1, bool constrain2);
	void onPenUp();

	void gestureEvent(QGestureEvent *event);
	void touchEvent(QTouchEvent *event);

	void resetCursor();

	inline void viewRectChanged() { emit viewRectChange(mapToScene(rect())); }

	CanvasViewShortcuts m_shortcuts;

	/**
	 * @brief State of the pen
	 *
	 * - NOTDOWN pen is not down
	 * - MOUSEDOWN mouse is down
	 * - TABLETDOWN tablet stylus is down
	 */
	enum {NOTDOWN, MOUSEDOWN, TABLETDOWN} m_pendown;

	enum class PenMode {
		Normal, Colorpick, Layerpick
	};

	NotificationBar *m_notificationBar;

	PenMode m_penmode;

	//! Is the view being dragged
	ViewDragMode m_dragmode;
	QPoint m_dragLastPoint;
	bool m_spacebar = false;

	//! Previous pointer location
	canvas::Point m_prevpoint;
	canvas::Point m_prevoutlinepoint;
	qreal m_pointerdistance;
	qreal m_pointervelocity;

	qreal m_gestureStartZoom;
	qreal m_gestureStartAngle;

	int m_outlineSize;
	bool m_showoutline, m_subpixeloutline, m_squareoutline;
	QCursor m_dotcursor, m_colorpickcursor, m_layerpickcursor, m_zoomcursor, m_rotatecursor;
	QCursor m_toolcursor;

	qreal m_zoom; // View zoom in percents
	qreal m_rotate; // View rotation in degrees
	bool m_flip; // Flip Y axis
	bool m_mirror; // Flip X axis

	drawingboard::CanvasScene *m_scene;

	// Input settings
	PressureMapping m_pressuremapping;

	int m_zoomWheelDelta;

	bool m_enableTablet;
	bool m_locked;
	bool m_pointertracking;
	bool m_pixelgrid;

	bool m_isFirstPoint;
	bool m_enableTouchScroll, m_enableTouchPinch, m_enableTouchTwist;
	bool m_touching, m_touchRotating;
	qreal m_touchStartZoom, m_touchStartRotate;
	qreal m_dpi;
	int m_brushCursorStyle;

	// On some Linux systems, the viewport doesn't properly trigger mouse enter
	// events through tablet move events, which causes the cursor to not update.
	// This hack leaves those events unaccepted, which causes a mouse move event
	// to be synthesized. This is picked up by the viewport and ignored by the
	// canvas view by checking for the synthetic flag.
	bool m_enableViewportEntryHack;
	qreal m_brushOutlineWidth;
};

}

#endif

