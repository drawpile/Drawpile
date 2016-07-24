/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef CANVASINPUTAREA_H
#define CANVASINPUTAREA_H

#include "core/point.h"
#include "tabletstate.h"
#include "pressure.h"

#include <QQuickItem>
#include <QCursor>

/**
 * @brief This item handles input (mouse, touch and tablet) for the canvas
 */
class CanvasInputArea : public QQuickItem
{
	Q_PROPERTY(qreal contentX READ contentX WRITE setContentX NOTIFY contentXChanged)
	Q_PROPERTY(qreal contentY READ contentY WRITE setContentY NOTIFY contentYChanged)
	Q_PROPERTY(qreal contentZoom READ contentZoom WRITE setContentZoom NOTIFY contentZoomChanged)
	Q_PROPERTY(qreal contentRotation READ contentRotation WRITE setContentRotation NOTIFY contentRotationChanged)

	Q_PROPERTY(bool locked READ isLocked WRITE setLocked NOTIFY isLockedChanged)
	Q_PROPERTY(QCursor toolCursor READ toolCursor WRITE setToolCursor NOTIFY toolCursorChanged)
	Q_PROPERTY(PressureMapping pressureMapping READ pressureMapping WRITE setPressureMapping NOTIFY pressureMappingChanged)

	Q_PROPERTY(QQuickItem* canvas READ canvas WRITE setCanvas NOTIFY canvasChanged)
	Q_PROPERTY(TabletState* tablet READ tabletState WRITE setTabletState NOTIFY tabletStateChanged)

	Q_OBJECT

public:
	explicit CanvasInputArea(QQuickItem *parent=nullptr);

	//! Map a point from scene coordinates to canvas coordinates
	Q_INVOKABLE QPointF mapToCanvas(const QPointF &point) const;
	Q_INVOKABLE inline paintcore::Point mapToCanvas(const QPointF &point, qreal	 pressure) const { return paintcore::Point(mapToCanvas(point), pressure); }

	QQuickItem *canvas() const { return m_canvas; }
	void setCanvas(QQuickItem *canvas) { m_canvas = canvas; emit canvasChanged(); }

	TabletState *tabletState() const { return m_tabletstate; }
	void setTabletState(TabletState *st) { m_tabletstate = st; emit tabletStateChanged(); }

	qreal contentX() const { return m_contentx; }
	void setContentX(qreal x) { m_contentx = x; emit contentXChanged(x); }

	qreal contentY() const { return m_contenty; }
	void setContentY(qreal y) { m_contenty = y; emit contentYChanged(y); }

	qreal contentZoom() const { return m_zoom; }
	void setContentZoom(qreal zoom);

	qreal contentRotation() const { return m_rotation; }
	void setContentRotation(qreal rotation) { m_rotation = rotation; emit contentRotationChanged(rotation); }

	bool isLocked() const { return m_locked; }
	void setLocked(bool lock);

	QCursor toolCursor() const { return m_toolcursor; }
	void setToolCursor(const QCursor &cursor);

	PressureMapping pressureMapping() const { return m_pressuremapping; }
	void setPressureMapping(const PressureMapping &pm) { m_pressuremapping = pm; emit pressureMappingChanged(); }

public slots:
	void zoomSteps(int steps);

signals:
	void canvasChanged();
	void tabletStateChanged();
	void contentXChanged(qreal);
	void contentYChanged(qreal);
	void contentZoomChanged(qreal);
	void contentRotationChanged(qreal);
	void contentWidthChanged(qreal);
	void contentHeightChanged(qreal);
	void isLockedChanged(bool);
	void toolCursorChanged();
	void pressureMappingChanged();

	void penDown(const QPointF &point, qreal pressure);
	void penMove(const QPointF &point, qreal pressure, bool shift, bool alt);
	void penUp();
	void quickAdjust(qreal value);

	void colorPickRequested(const QPointF &point);

protected:
	void mouseMoveEvent(QMouseEvent *event);
	void hoverMoveEvent(QHoverEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void hoverEnterEvent(QHoverEvent *event);
	void wheelEvent(QWheelEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);
	bool event(QEvent *event);

private:
	void nativeGestureEvent(QNativeGestureEvent *event);

	void resetCursor();

	// Dragging modes
	enum ViewTransform {DRAG_NOTRANSFORM, DRAG_TRANSLATE, DRAG_ROTATE, DRAG_ZOOM, DRAG_QUICKADJUST1};

	// unified mouse/stylus event handlers
	void penPressEvent(const QPointF &pos, float pressure, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus);
	void penMoveEvent(const QPointF &pos, float pressure, Qt::KeyboardModifiers modifiers, bool isStylus);
	void penReleaseEvent(const QPointF &pos, Qt::MouseButton button);

	qreal mapPressure(qreal pressure, bool stylus);

	// View drag handlers
	void startDrag(const QPointF &pos, ViewTransform mode);
	void moveDrag(const QPointF &pos);
	void stopDrag();

	QQuickItem *m_canvas;
	TabletState *m_tabletstate;

	// View settings
	bool m_enableTouchScroll, m_enableTouchPinch, m_enableTouchRotate;
	PressureMapping m_pressuremapping;

	// Cursors
	const QCursor m_colorPickerCursor;
	const QCursor m_crosshairCursor;

	// View state
	bool m_locked;
	QCursor m_toolcursor;

	// View transformation state
	qreal m_contentx;
	qreal m_contenty;
	qreal m_rotation;
	qreal m_zoom;

	// Input device state
	bool m_specialpenmode;

	ViewTransform m_isdragging;
	ViewTransform m_dragbtndown;
	qreal m_dragx, m_dragy;

	QPointF m_prevpoint;
	paintcore::Point m_prevoutlinepoint;
	qreal m_pointerdistance;
	qreal m_pointervelocity;

	bool m_prevshift;
	bool m_prevalt;

	int m_zoomWheelDelta;

	bool m_pendown;
	bool m_touching;

};

#endif // CANVASINPUTAREA_H
