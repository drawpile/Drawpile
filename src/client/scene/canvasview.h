/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include <QGraphicsView>

class QGestureEvent;
class QTouchEvent;

#include "core/point.h"
#include "tools/tool.h"
#include "canvas/pressure.h"

namespace drawingboard {
	class CanvasScene;
}

namespace widgets {


/**
 * @brief Editor view
 * The editor view is a customized QGraphicsView that displays
 * the drawing board and handes user input.
 * It also provides other features, such as brush outline preview.
 */
class CanvasView : public QGraphicsView
{
	Q_OBJECT
	public:
		enum TabletMode { DISABLE_TABLET, ENABLE_TABLET, HYBRID_TABLET }; // hybrid mode combines tablet and mouse events to work around QTabletEvent bugs

		CanvasView(QWidget *parent=0);

		//! Set the board to use
		void setCanvas(drawingboard::CanvasScene *scene);
		
		//! Get the current zoom factor
		qreal zoom() const { return _zoom; }

		//! Get the current rotation angle in degrees
		qreal rotation() const { return _rotate; }
		
		using QGraphicsView::mapToScene;
		paintcore::Point mapToScene(const QPoint &point, qreal pressure) const;
		paintcore::Point mapToScene(const QPointF &point, qreal pressure) const;

		//! The center point of the view in scene coordinates
		QPoint viewCenterPoint() const;

		//! Enable/disable tablet event handling
		void setTabletMode(TabletMode mode);

		//! Enable/disable touch gestures
		void setTouchGestures(bool scroll, bool pinch, bool twist);

		//! Is drawing in progress at the moment?
		bool isPenDown() const { return _pendown != NOTDOWN; }

		//! Is this point (scene coordinates) inside the viewport?
		bool isPointVisible(const QPointF &point) const;

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

		//! Pointer was just brought to the top of the widget border
		void hotBorder(bool hot);

		void penDown(const QPointF &point, qreal pressure);
		void penMove(const QPointF &point, qreal pressure, bool shift, bool alt);
		void penUp();
		void quickAdjust(qreal value);

	public slots:
		//! Set the size of the brush preview outline
		void setOutlineSize(int size);

		//! Enable subpixel precision for brush preview outline
		void setOutlineSubpixelMode(bool subpixel);

		//! Enable or disable the crosshair cursor
		void setCrosshair(bool enable);

		//! Enable or disable pixel grid (shown only at high zoom levels)
		void setPixelGrid(bool enable);

		//! Scroll view to location
		void scrollTo(const QPoint& point);
		
		//! Set the zoom factor
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

		void setToolCursor(const QCursor &cursor);

	protected:
		void enterEvent(QEvent *event);
		void leaveEvent(QEvent *event);
		void mouseMoveEvent(QMouseEvent *event);
		void mousePressEvent(QMouseEvent *event);
		void mouseReleaseEvent(QMouseEvent *event);
		void mouseDoubleClickEvent(QMouseEvent*);
		void wheelEvent(QWheelEvent *event);
		void keyPressEvent(QKeyEvent *event);
		void keyReleaseEvent(QKeyEvent *event);
		bool viewportEvent(QEvent *event);
		void drawForeground(QPainter *painter, const QRectF& rect);
		void dragEnterEvent(QDragEnterEvent *event);
		void dragMoveEvent(QDragMoveEvent *event);
		void dropEvent(QDropEvent *event);
		void showEvent(QShowEvent *event);

	private:
		// unified mouse/stylus event handlers
		void penPressEvent(const QPointF &pos, float pressure, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus);
		void penMoveEvent(const QPointF &pos, float pressure, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool isStylus);
		void penReleaseEvent(const QPointF &pos, Qt::MouseButton button);

		void doQuickAdjust1(float delta);

	private:
		float mapPressure(float pressure, bool stylus);

		//! View transformation mode (for dragging)
		enum ViewTransform {DRAG_NOTRANSFORM, DRAG_TRANSLATE, DRAG_ROTATE, DRAG_ZOOM, DRAG_QUICKADJUST1};

		//! Start dragging the view
		void startDrag(int x, int y, ViewTransform mode);

		//! Stop dragging the view
		void stopDrag();

		//! Drag the view
		void moveDrag(int x, int y);

		//! Redraw the scene around the outline cursor if necesasry
		void updateOutline(paintcore::Point point);
		void updateOutline();

		void onPenDown(const paintcore::Point &p, bool right);
		void onPenMove(const paintcore::Point &p, bool right, bool shift, bool alt);
		void onPenUp(bool right);
		
		void gestureEvent(QGestureEvent *event);
		void touchEvent(QTouchEvent *event);

		void resetCursor();

		/**
		 * @brief State of the pen
		 *
		 * - NOTDOWN pen is not down
		 * - MOUSEDOWN mouse is down
		 * - TABLETDOWN tablet stylus is down
		 */
		enum {NOTDOWN, MOUSEDOWN, TABLETDOWN} _pendown;

		//! If Ctrl is held, pen goes to "special" mode (which is currently quick color picker mode)
		bool _specialpenmode;

		//! Is the view being dragged
		ViewTransform _isdragging;
		ViewTransform _dragbtndown;
		int _dragx,_dragy;

		//! Previous pointer location
		paintcore::Point m_firstpoint;
		paintcore::Point _prevpoint;
		paintcore::Point _prevoutlinepoint;
		qreal _pointerdistance;
		qreal _pointervelocity;
		bool _prevshift;
		bool _prevalt;

		qreal _gestureStartZoom;
		qreal _gestureStartAngle;

		float _outlinesize;
		bool _showoutline, _subpixeloutline;
		bool _enablecrosshair;
		QCursor _crosshaircursor, _colorpickcursor;
		QCursor m_toolcursor;

		qreal _zoom; // View zoom in percents
		qreal _rotate; // View rotation in degrees
		bool _flip; // Flip Y axis
		bool _mirror; // Flip X axis

		drawingboard::CanvasScene *_scene;

		// Input settings
		PressureMapping m_pressuremapping;

		TabletMode _tabletmode;

		qreal _lastPressure; // these two are used only with hybrid tablet mode
		bool _stylusDown;

		int _zoomWheelDelta;

		bool _locked;
		bool _pointertracking;
		bool _pixelgrid;

		bool _hotBorderTop;

		bool _enableTouchScroll, _enableTouchPinch, _enableTouchTwist;
		bool _touching, _touchRotating;
		qreal _touchStartZoom, _touchStartRotate;
		qreal _dpi;
};

}

#endif

