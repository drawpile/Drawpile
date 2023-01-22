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

#include "canvasview.h"
#include "canvasscene.h"
#include "canvas/canvasmodel.h"

#include "notifications.h"

#include "widgets/notifbar.h"
#include "utils/qtguicompat.h"

#include <QMouseEvent>
#include <QTabletEvent>
#include <QScrollBar>
#include <QPainter>
#include <QMimeData>
#include <QApplication>
#include <QGestureEvent>
#include <QSettings>
#include <QWindow>
#include <QScreen>
#include <QtMath>

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), m_pendown(NOTDOWN), m_penmode(PenMode::Normal), m_dragmode(ViewDragMode::None),
	m_outlineSize(2), m_showoutline(true), m_subpixeloutline(true), m_squareoutline(false),
	m_zoom(100), m_rotate(0), m_flip(false), m_mirror(false),
	m_scene(nullptr),
	m_zoomWheelDelta(0),
	m_enableTablet(true),
	m_locked(false), m_pointertracking(false), m_pixelgrid(true),
	m_isFirstPoint(false),
	m_enableTouchScroll(true), m_enableTouchPinch(true), m_enableTouchTwist(true),
	m_touching(false), m_touchRotating(false),
	m_dpi(96),
	m_brushCursorStyle(0),
	m_enableViewportEntryHack(false),
	m_brushOutlineWidth(1.0)
{
	viewport()->setAcceptDrops(true);
#ifdef Q_OS_MACOS // Standard touch events seem to work better with mac touchpad
	viewport()->grabGesture(Qt::PinchGesture);
#else
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
#endif
	viewport()->setMouseTracking(true);
	setAcceptDrops(true);

	setBackgroundBrush(QColor(100,100,100));

	m_notificationBar = new NotificationBar(this);
	connect(m_notificationBar, &NotificationBar::actionButtonClicked, this, &CanvasView::reconnectRequested);

	m_colorpickcursor = QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29);
	m_layerpickcursor = QCursor(QPixmap(":/cursors/layerpicker.png"), 2, 29);
	m_zoomcursor = QCursor(QPixmap(":/cursors/zoom.png"), 8, 8);
	m_rotatecursor = QCursor(QPixmap(":/cursors/rotate.png"), 16, 16);

	// Generate the minimalistic dot cursor
	{
		QPixmap dot(8, 8);
		dot.fill(Qt::transparent);
		QPainter p(&dot);
		p.setPen(Qt::white);
		p.drawPoint(0, 0);
		m_dotcursor = QCursor(dot, 0, 0);
	}

	updateShortcuts();
}

void CanvasView::showDisconnectedWarning(const QString &message)
{
	m_notificationBar->show(message, tr("Reconnect"), NotificationBar::RoleColor::Warning);
}

void CanvasView::updateShortcuts()
{
	QSettings cfg;
	cfg.beginGroup("settings/canvasShortcuts");
	m_shortcuts = CanvasViewShortcuts::load(cfg);
}
void CanvasView::setCanvas(drawingboard::CanvasScene *scene)
{
	m_scene = scene;
	setScene(scene);

	connect(m_scene, &drawingboard::CanvasScene::canvasResized, this, [this](int xoff, int yoff, const QSize &oldsize) {
		if(oldsize.isEmpty()) {
			centerOn(m_scene->sceneRect().center());
		} else {
			const qreal z = m_zoom / 100.0;
			scrollBy(xoff * z, yoff * z);
		}
		viewRectChanged();
	});

	connect(m_scene, &drawingboard::CanvasScene::paintEngineCrashed, this, [this]() {
		m_notificationBar->show(
			tr("Paint engine has crashed! Save your work and restart the application."),
			QString(),
			NotificationBar::RoleColor::Fatal
		);
	});

	viewRectChanged();
}

void CanvasView::scrollBy(int x, int y)
{
	horizontalScrollBar()->setValue(horizontalScrollBar()->value() + x);
	verticalScrollBar()->setValue(verticalScrollBar()->value() + y);
}

void CanvasView::zoomSteps(int steps)
{
	if(m_zoom<100 || (m_zoom==100 && steps<0))
		setZoom(qRound(qMin(qreal(100), m_zoom + steps * 10) / 10) * 10);
	else
		setZoom(qRound(qMax(qreal(100), m_zoom + steps * 50) / 50) * 50);
	viewRectChanged();
}

void CanvasView::zoomin()
{
	zoomSteps(1);
}

void CanvasView::zoomout()
{
	zoomSteps(-1);
}

void CanvasView::zoomTo(const QRect &rect, int steps)
{
	centerOn(rect.center());

	if(rect.width() < 15 || rect.height() < 15 || steps < 0) {
		zoomSteps(steps);

	} else {
		const auto viewRect = mapFromScene(rect).boundingRect();
		const qreal xScale = qreal(viewport()->width()) / viewRect.width();
		const qreal yScale = qreal(viewport()->height()) / viewRect.height();
		setZoom(m_zoom * qMin(xScale, yScale));
	}
}

qreal CanvasView::fitToWindowScale() const
{
	if(!m_scene || !m_scene->hasImage())
		return 100;

	const auto s = m_scene->model()->size();

	const qreal xScale = qreal(viewport()->width()) / s.width();
	const qreal yScale = qreal(viewport()->height()) / s.height();

	return qMin(xScale, yScale);
}

void CanvasView::zoomToFit()
{
	if(!m_scene || !m_scene->hasImage())
		return;

	const QRect r {
		QPoint(),
		m_scene->model()->size()
	};

	const qreal xScale = qreal(viewport()->width()) / r.width();
	const qreal yScale = qreal(viewport()->height()) / r.height();

	centerOn(r.center());
	setZoom(qMin(xScale, yScale) * 100);
}

/**
 * You should use this function instead of calling scale() directly
 * to keep track of the zoom factor.
 * @param zoom new zoom factor
 */
void CanvasView::setZoom(qreal zoom)
{
	if(zoom<=0)
		return;

	m_zoom = zoom;
	QTransform t(1,0,0,1, transform().dx(), transform().dy());
	t.scale(m_zoom/100.0, m_zoom/100.0);
	t.rotate(m_rotate);

	t.scale(m_mirror ? -1 : 1, m_flip ? -1 : 1);

	setTransform(t);

	// Enable smooth scaling when under 200% zoom, because nearest-neighbour
	// interpolation just doesn't look good in that range.
	// Also enable when rotating, since that tends to cause terrible jaggies
	setRenderHint(
		QPainter::SmoothPixmapTransform,
		m_zoom < 200 || (m_zoom < 800 && int(m_rotate) % 90)
		);

	emit viewTransformed(m_zoom, m_rotate);
}

/**
 * You should use this function instead calling rotate() directly
 * to keep track of the rotation angle.
 * @param angle new rotation angle
 */
void CanvasView::setRotation(qreal angle)
{
	m_rotate = angle;
	setZoom(m_zoom);
}

void CanvasView::setViewFlip(bool flip)
{
	if(flip != m_flip) {
		m_flip = flip;
		setZoom(m_zoom);
	}
}

void CanvasView::setViewMirror(bool mirror)
{
	if(mirror != m_mirror) {
		m_mirror = mirror;
		setZoom(m_zoom);
	}
}

void CanvasView::setLocked(bool lock)
{
	if(lock && !m_locked)
		notification::playSound(notification::Event::LOCKED);
	else if(!lock && m_locked)
		notification::playSound(notification::Event::UNLOCKED);

	m_locked = lock;
	resetCursor();
}

void CanvasView::setBrushCursorStyle(int style, qreal outlineWidth)
{
	m_brushCursorStyle = style;
	m_brushOutlineWidth = qIsNaN(outlineWidth) || outlineWidth < 1.0 ? 1.0 : outlineWidth;
	resetCursor();
}

void CanvasView::setToolCursor(const QCursor &cursor)
{
	m_toolcursor = cursor;
	resetCursor();
}

void CanvasView::resetCursor()
{
	if(m_dragmode == ViewDragMode::Prepared) {
		viewport()->setCursor(Qt::OpenHandCursor);
		return;
	} else if(m_dragmode == ViewDragMode::Started) {
		const int shortcut = CanvasViewShortcuts::matches(QApplication::queryKeyboardModifiers(),
			m_shortcuts.dragZoom,
			m_shortcuts.dragRotate,
			m_shortcuts.dragQuickAdjust
		);
		switch(shortcut) {
			case 0: viewport()->setCursor(m_zoomcursor); break;
			case 1: viewport()->setCursor(m_rotatecursor); break;
			case 2: viewport()->setCursor(Qt::SplitVCursor); break;
			default: viewport()->setCursor(Qt::ClosedHandCursor); break;
		}
		return;
	}

	switch(m_penmode) {
	case PenMode::Normal: break;
	case PenMode::Colorpick: viewport()->setCursor(m_colorpickcursor); return;
	case PenMode::Layerpick: viewport()->setCursor(m_layerpickcursor); return;
	}

	if(m_locked) {
		viewport()->setCursor(Qt::ForbiddenCursor);
		return;
	}

	if(m_toolcursor.shape() == Qt::CrossCursor) {
		switch(m_brushCursorStyle) {
		case 0: viewport()->setCursor(m_dotcursor); return;
		case 1: viewport()->setCursor(Qt::CrossCursor); return;
		default: viewport()->setCursor(Qt::ArrowCursor); return;
		}
	}

	viewport()->setCursor(m_toolcursor);
}

void CanvasView::setPixelGrid(bool enable)
{
	m_pixelgrid = enable;
	viewport()->update();
}

/**
 * @param radius circle radius
 */
void CanvasView::setOutlineSize(int newSize)
{
	if(m_showoutline && (m_outlineSize>0 || newSize>0)) {
		const qreal maxSize = qMax(m_outlineSize, newSize) + m_brushOutlineWidth;
		const qreal maxRad = maxSize / 2.0;
		QList<QRectF> rect;
		rect.append(QRectF {
					m_prevoutlinepoint.x() - maxRad,
					m_prevoutlinepoint.y() - maxRad,
					maxSize,
					maxSize
					});
		updateScene(rect);
	}
	m_outlineSize = newSize;
}

void CanvasView::setOutlineMode(bool subpixel, bool square)
{
	m_subpixeloutline = subpixel;
	m_squareoutline = square;
}

void CanvasView::drawForeground(QPainter *painter, const QRectF& rect)
{
	if(m_pixelgrid && m_zoom >= 800) {
		QPen pen(QColor(160, 160, 160));
		pen.setCosmetic(true);
		painter->setPen(pen);
		for(int x=rect.left();x<=rect.right();++x) {
			painter->drawLine(x, rect.top(), x, rect.bottom()+1);
		}

		for(int y=rect.top();y<=rect.bottom();++y) {
			painter->drawLine(rect.left(), y, rect.right()+1, y);
		}
	}
	if(m_showoutline && m_outlineSize>0 && m_penmode == PenMode::Normal && m_dragmode == ViewDragMode::None && !m_locked) {
		QRectF outline(m_prevoutlinepoint-QPointF(m_outlineSize/2.0, m_outlineSize/2.0),
					QSizeF(m_outlineSize, m_outlineSize));

		if(!m_subpixeloutline && m_outlineSize%2==0)
			outline.translate(-0.5, -0.5);

		if(rect.intersects(outline)) {
			painter->save();
			QPen pen(QColor(96, 191, 96));
			pen.setCosmetic(true);
			pen.setWidthF(m_brushOutlineWidth);
			painter->setPen(pen);
			painter->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
			if(m_squareoutline)
				painter->drawRect(outline);
			else
				painter->drawEllipse(outline);
			painter->restore();
		}
	}
}

void CanvasView::setEnableViewportEntryHack(bool enabled)
{
	m_enableViewportEntryHack = enabled;
}

void CanvasView::enterEvent(compat::EnterEvent *event)
{
	QGraphicsView::enterEvent(event);
	m_showoutline = true;

	// Give focus to this widget on mouseover. This is so that
	// using spacebar for dragging works rightaway. Avoid stealing
	// focus from text edit widgets though.
	QWidget *oldfocus = QApplication::focusWidget();
	if(!oldfocus || !(
		oldfocus->inherits("QLineEdit") ||
		oldfocus->inherits("QTextEdit") ||
		oldfocus->inherits("QPlainTextEdit"))
		) {
		setFocus(Qt::MouseFocusReason);
	}
}

void CanvasView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	m_showoutline = false;
	updateOutline();
}

canvas::Point CanvasView::mapToScene(const QPoint &point, qreal pressure) const
{
	return canvas::Point(mapToScene(point), pressure);
}

canvas::Point CanvasView::mapToScene(const QPointF &point, qreal pressure) const
{
	// QGraphicsView API lacks mapToScene(QPointF), even though
	// the QPoint is converted to QPointF internally...
	// To work around this, map (x,y) and (x+1, y+1) and linearly interpolate
	// between the two
	double tmp;
	qreal xf = qAbs(modf(point.x(), &tmp));
	qreal yf = qAbs(modf(point.y(), &tmp));

	QPoint p0(floor(point.x()), floor(point.y()));
	QPointF p1 = mapToScene(p0);
	QPointF p2 = mapToScene(p0 + QPoint(1,1));

	QPointF mapped(
		(p1.x()-p2.x()) * xf + p2.x(),
		(p1.y()-p2.y()) * yf + p2.y()
	);

	return canvas::Point(mapped, pressure);
}

void CanvasView::setPointerTracking(bool tracking)
{
	m_pointertracking = tracking;
	if(!tracking && m_scene) {
		// TODO
		//_scene->hideUserMarker();
	}
}

void CanvasView::setPressureMapping(const PressureMapping &mapping)
{
	m_pressuremapping = mapping;
}

void CanvasView::onPenDown(const canvas::Point &p, bool right)
{
	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_locked)
				emit penDown(p, p.pressure(), right, m_zoom / 100.0);
			break;
		case PenMode::Colorpick:
			m_scene->model()->pickColor(p.x(), p.y(), 0, 0);
			break;
		case PenMode::Layerpick:
			m_scene->model()->pickLayer(p.x(), p.y());
			break;
		}
	}
}

void CanvasView::onPenMove(const canvas::Point &p, bool right, bool constrain1, bool constrain2)
{
	Q_UNUSED(right)

	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_locked)
				emit penMove(p, p.pressure(), constrain1, constrain2);
			break;
		case PenMode::Colorpick:
			m_scene->model()->pickColor(p.x(), p.y(), 0, 0);
			break;
		case PenMode::Layerpick:
			m_scene->model()->pickLayer(p.x(), p.y());
			break;
		}
	}
}

void CanvasView::onPenUp()
{
	if(!m_locked && m_penmode == PenMode::Normal)
		emit penUp();

	m_penmode = PenMode::Normal;
	resetCursor();
	updateOutline();
}

void CanvasView::penPressEvent(const QPointF &pos, qreal pressure, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(m_pendown != NOTDOWN)
		return;

	if((button == Qt::MiddleButton && m_dragmode != ViewDragMode::Started) || m_dragmode == ViewDragMode::Prepared) {
		m_dragLastPoint = pos.toPoint();
		m_dragmode = ViewDragMode::Started;

		resetCursor();
		updateOutline();

	} else if((button == Qt::LeftButton || button == Qt::RightButton) && m_dragmode==ViewDragMode::None) {
		m_pendown = isStylus ? TABLETDOWN : MOUSEDOWN;
		m_pointerdistance = 0;
		m_pointervelocity = 0;
		m_prevpoint = mapToScene(pos, pressure);

		m_penmode = PenMode(CanvasViewShortcuts::matches(
			modifiers,
			m_shortcuts.colorPick, m_shortcuts.layerPick
		) + 1);

		onPenDown(mapToScene(pos, mapPressure(pressure, isStylus)), button == Qt::RightButton);
	}
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	if(m_touching)
		return;

	penPressEvent(
		event->pos(),
		1,
		event->button(),
		event->modifiers(),
		false
	);
}

void CanvasView::penMoveEvent(const QPointF &pos, qreal pressure, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(m_dragmode == ViewDragMode::Started) {
		moveDrag(pos.toPoint(), modifiers);

	} else {
		canvas::Point point = mapToScene(pos, pressure);
		updateOutline(point);
		if(!m_prevpoint.intSame(point)) {
			if(m_pendown) {
				m_pointervelocity = point.distance(m_prevpoint);
				m_pointerdistance += m_pointervelocity;
				point.setPressure(mapPressure(pressure, isStylus));
				onPenMove(
					point,
					buttons.testFlag(Qt::RightButton),
					m_shortcuts.toolConstraint1.matches(modifiers),
					m_shortcuts.toolConstraint2.matches(modifiers)
				);

			} else {
				emit penHover(point);
				if(m_pointertracking && m_scene->hasImage())
					emit pointerMoved(point);
			}
			m_prevpoint = point;
		}
	}
}

//! Handle mouse motion events
void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	if(m_enableViewportEntryHack && (event->source() & Qt::MouseEventSynthesizedByQt))
		return;
	if(m_pendown == TABLETDOWN)
		return;
	if(m_touching)
		return;
	if(m_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	penMoveEvent(
		event->pos(),
		1.0,
		event->buttons(),
		event->modifiers(),
		false
	);
}

void CanvasView::penReleaseEvent(const QPointF &pos, Qt::MouseButton button)
{
	m_prevpoint = mapToScene(pos, 0.0);
	if(m_dragmode != ViewDragMode::None) {
		if(m_spacebar)
			m_dragmode = ViewDragMode::Prepared;
		else
			m_dragmode = ViewDragMode::None;

		resetCursor();

	} else if(m_pendown == TABLETDOWN || ((button == Qt::LeftButton || button == Qt::RightButton) && m_pendown == MOUSEDOWN)) {
		onPenUp();
		m_pendown = NOTDOWN;
	}
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	if(m_touching)
		return;
	penReleaseEvent(event->pos(), event->button());
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent*)
{
	// Ignore doubleclicks
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	const int shortcut = CanvasViewShortcuts::matches(event->modifiers(),
		m_shortcuts.scrollRotate,
		m_shortcuts.scrollZoom,
		m_shortcuts.scrollQuickAdjust
	);

	switch(shortcut) {
	case 0:
	case 1: {
		event->accept();
		if(event->angleDelta().y() == 0)
			m_zoomWheelDelta += event->angleDelta().x();
		else
			m_zoomWheelDelta += event->angleDelta().y();
		const int steps=m_zoomWheelDelta / 120;
		m_zoomWheelDelta -= steps * 120;

		if(steps != 0) {
			if(shortcut == 0)
				setRotation(rotation() + steps * 10);
			else
				zoomSteps(steps);
		}
		break;
		}

	case 2:
		event->accept();
		emit quickAdjust(event->angleDelta().y() / (30 * 4.0));
		break;

	default:
		QGraphicsView::wheelEvent(event);
	}
}

void CanvasView::keyPressEvent(QKeyEvent *event) {
	if(event->key() == Qt::Key_Space) {
		event->accept();
		if(!event->isAutoRepeat() && m_dragmode == ViewDragMode::None && m_pendown == NOTDOWN) {
			m_dragmode = ViewDragMode::Prepared;
			m_spacebar = true;
			updateOutline();
		}

	} else {
		QGraphicsView::keyPressEvent(event);

		if(m_pendown == NOTDOWN) {
			// Switch penmode here so we get to see the right cursor
			m_penmode = PenMode(CanvasViewShortcuts::matches(
				event->modifiers(),
				m_shortcuts.colorPick, m_shortcuts.layerPick
			) + 1);
		}
	}

	resetCursor();
}

void CanvasView::keyReleaseEvent(QKeyEvent *event) {
	QGraphicsView::keyReleaseEvent(event);

	if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
		m_spacebar = false;
		if(m_dragmode == ViewDragMode::Prepared) {
			m_dragmode = ViewDragMode::None;
			updateOutline();
		}
	}

	if(m_pendown == NOTDOWN) {
		// Switch penmode here so we get to see the right cursor
		m_penmode = PenMode(CanvasViewShortcuts::matches(
			event->modifiers(),
			m_shortcuts.colorPick, m_shortcuts.layerPick
		) + 1);
	}

	resetCursor();
}

void CanvasView::gestureEvent(QGestureEvent *event)
{
	QPinchGesture *pinch = static_cast<QPinchGesture*>(event->gesture(Qt::PinchGesture));
	if(pinch) {
		if(pinch->state() == Qt::GestureStarted) {
			m_gestureStartZoom = m_zoom;
			m_gestureStartAngle = m_rotate;
		}

		if(m_enableTouchPinch && (pinch->changeFlags() & QPinchGesture::ScaleFactorChanged))
			setZoom(m_gestureStartZoom * pinch->totalScaleFactor());

		if(m_enableTouchTwist && (pinch->changeFlags() & QPinchGesture::RotationAngleChanged))
			setRotation(m_gestureStartAngle + pinch->totalRotationAngle());
	}
}

static qreal squareDist(const QPointF &p)
{
	return p.x()*p.x() + p.y()*p.y();
}

void CanvasView::setTouchGestures(bool scroll, bool pinch, bool twist)
{
	m_enableTouchScroll = scroll;
	m_enableTouchPinch = pinch;
	m_enableTouchTwist = twist;
}

void CanvasView::touchEvent(QTouchEvent *event)
{
	event->accept();

	switch(event->type()) {
	case QEvent::TouchBegin:
		m_touchRotating = false;
		break;

	case QEvent::TouchUpdate: {
		QPointF startCenter, lastCenter, center;
		const int points = compat::touchPoints(*event).size();
		for(const auto &tp : compat::touchPoints(*event)) {
			startCenter += compat::touchStartPos(tp);
			lastCenter += compat::touchLastPos(tp);
			center += compat::touchPos(tp);
		}
		startCenter /= points;
		lastCenter /= points;
		center /= points;

		if(!m_touching) {
			m_touchStartZoom = zoom();
			m_touchStartRotate = rotation();
		}

		// Single finger drag when touch scroll is enabled,
		// but also drag with a pinch gesture. Single finger drag
		// may be deactivated to support finger painting.
		if(m_enableTouchScroll || (m_enableTouchPinch && points >= 2)) {
			m_touching = true;
			float dx = center.x() - lastCenter.x();
			float dy = center.y() - lastCenter.y();
			horizontalScrollBar()->setValue(horizontalScrollBar()->value() - dx);
			verticalScrollBar()->setValue(verticalScrollBar()->value() - dy);
		}

		// Scaling and rotation with two fingers
		if(points >= 2 && (m_enableTouchPinch | m_enableTouchTwist)) {
			m_touching = true;
			float startAvgDist=0, avgDist=0;
			for(const auto &tp : compat::touchPoints(*event)) {
				startAvgDist += squareDist(compat::touchStartPos(tp) - startCenter);
				avgDist += squareDist(compat::touchPos(tp) - center);
			}
			startAvgDist = sqrt(startAvgDist);

			if(m_enableTouchPinch) {
				avgDist = sqrt(avgDist);
				const qreal dZoom = avgDist / startAvgDist;
				m_zoom = m_touchStartZoom * dZoom;
			}

			if(m_enableTouchTwist) {
				const auto &tps = compat::touchPoints(*event);

				const QLineF l1 { compat::touchStartPos(tps.first()), compat::touchStartPos(tps.last()) };
				const QLineF l2 { compat::touchPos(tps.first()), compat::touchPos(tps.last()) };

				const qreal dAngle = l1.angle() - l2.angle();

				// Require a small nudge to activate rotation to avoid rotating when the user just wanted to zoom
				// Alsom, only rotate when touch points start out far enough from each other. Initial angle measurement
				// is inaccurate when touchpoints are close together.
				if(startAvgDist / m_dpi > 0.8 && (qAbs(dAngle) > 3.0 || m_touchRotating)) {
					m_touchRotating = true;
					m_rotate = m_touchStartRotate + dAngle;
				}

			}

			// Recalculate view matrix
			setZoom(zoom());
		}

	} break;

	case QEvent::TouchEnd:
	case QEvent::TouchCancel:
		m_touching = false;
		break;
	default: break;
	}
}

//! Handle viewport events
/**
 * Tablet events are handled here
 * @param event event info
 */
bool CanvasView::viewportEvent(QEvent *event)
{
	if(event->type() == QEvent::Gesture) {
		gestureEvent(static_cast<QGestureEvent*>(event));
	}
#ifndef Q_OS_MACOS // On Mac, the above gesture events work better
	else if(event->type()==QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
		touchEvent(static_cast<QTouchEvent*>(event));
	}
#endif
	else if(event->type() == QEvent::TabletPress && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);

		// Note: it is possible to get a mouse press event for a tablet event (even before
		// the tablet event is received or even though tabev->accept() is called), but
		// it is never possible to get a TabletPress for a real mouse press. Therefore,
		// we don't actually do anything yet in the penDown handler other than remember
		// the initial point and we'll let a TabletEvent override the mouse event.
		tabev->accept();

		penPressEvent(
			compat::tabPosF(*tabev),
			tabev->pressure(),
			tabev->button(),
			QApplication::queryKeyboardModifiers(), // TODO check if tablet event modifiers() is still broken in Qt 5.12
			true
		);
	}
	else if(event->type() == QEvent::TabletMove && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		if (!m_enableViewportEntryHack)
			tabev->accept();

		penMoveEvent(
			compat::tabPosF(*tabev),
			tabev->pressure(),
			tabev->buttons(),
			QApplication::queryKeyboardModifiers(), // TODO check if tablet event modifiers() is still broken in Qt 5.12
			true
		);
	}
	else if(event->type() == QEvent::TabletRelease && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		tabev->accept();
		penReleaseEvent(compat::tabPosF(*tabev), tabev->button());
	}
	else {
		return QGraphicsView::viewportEvent(event);
	}

	return true;
}

qreal CanvasView::mapPressure(qreal pressure, bool stylus)
{
	switch(m_pressuremapping.mode) {
	case PressureMapping::STYLUS:
		return stylus ? m_pressuremapping.curve.value(pressure) : 1.0;

	case PressureMapping::DISTANCE: {
		const qreal scale = 10 + m_pressuremapping.param * 190.0;
		return m_pressuremapping.curve.value(m_pointerdistance / scale);
	}

	case PressureMapping::VELOCITY: {
		const qreal scale = 10 + m_pressuremapping.param * 90.0;
		return m_pressuremapping.curve.value(m_pointervelocity / scale);
	}
	}

	// Shouldn't be reached
	Q_ASSERT(false);
	return 0;
}

void CanvasView::updateOutline(canvas::Point point) {
	if(!m_subpixeloutline) {
		point.setX(qFloor(point.x()) + 0.5);
		point.setY(qFloor(point.y()) + 0.5);
	}
	if(m_showoutline && !m_locked && !point.roughlySame(m_prevoutlinepoint)) {
		QList<QRectF> rect;
		const qreal owidth = m_outlineSize + m_brushOutlineWidth;
		const qreal orad = owidth / 2.0;
		rect.append(QRectF(
					m_prevoutlinepoint.x() - orad,
					m_prevoutlinepoint.y() - orad,
					owidth,
					owidth
				));
		rect.append(QRectF(
						point.x() - orad,
						point.y() - orad,
						owidth,
						owidth
					));
		updateScene(rect);
		m_prevoutlinepoint = point;
	}
}

void CanvasView::updateOutline()
{
	QList<QRectF> rect;
	const qreal owidth = m_outlineSize + m_brushOutlineWidth;
	const qreal orad = owidth / 2.0;

	rect.append(QRectF(
		m_prevoutlinepoint.x() - orad,
		m_prevoutlinepoint.y() - orad,
		owidth,
		owidth
	));
	updateScene(rect);

}

QPoint CanvasView::viewCenterPoint() const
{
	return mapToScene(rect().center()).toPoint();
}

bool CanvasView::isPointVisible(const QPointF &point) const
{
	QPoint p = mapFromScene(point);
	return p.x() > 0 && p.y() > 0 && p.x() < width() && p.y() < height();
}

void CanvasView::scrollTo(const QPoint& point)
{
	centerOn(point);
}

/**
 * @param x x coordinate
 * @param y y coordinate
 */
void CanvasView::moveDrag(const QPoint &point, Qt::KeyboardModifiers modifiers)
{
	const int dx = m_dragLastPoint.x() - point.x();
	const int dy = m_dragLastPoint.y() - point.y();

	const int shortcut = CanvasViewShortcuts::matches(modifiers,
		m_shortcuts.dragRotate,
		m_shortcuts.dragZoom,
		m_shortcuts.dragQuickAdjust
		);

	switch(shortcut) {
	case 0: {
		const qreal preva = qAtan2(width()/2 - m_dragLastPoint.x(), height()/2 - m_dragLastPoint.y());
		const qreal a = qAtan2(width()/2 - point.x(), height()/2 - point.y());
		setRotation(rotation() + qRadiansToDegrees(preva-a));
		break;
		}

	case 1:
		if(dy!=0) {
			const auto delta = qBound(-1.0, dy / 100.0, 1.0);
			if(delta>0) {
				setZoom(m_zoom * (1+delta));
			} else if(delta<0) {
				setZoom(m_zoom / (1-delta));
			}
		}
		break;

	case 2:
		emit quickAdjust(qBound(-2.0, dy / 10.0, 2.0));
		break;

	default: {
		QScrollBar *ver = verticalScrollBar();
		QScrollBar *hor = horizontalScrollBar();
		ver->setSliderPosition(ver->sliderPosition()+dy);
		hor->setSliderPosition(hor->sliderPosition()+dx);
		}
	}

	m_dragLastPoint = point;
}

/**
 * @brief accept image drops
 * @param event event info
 *
 * @todo Check file extensions
 */
void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage() || event->mimeData()->hasColor())
		event->acceptProposedAction();
}

void CanvasView::dragMoveEvent(QDragMoveEvent *event)
{
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage())
		event->acceptProposedAction();
}

/**
 * @brief handle image drops
 * @param event event info
 */
void CanvasView::dropEvent(QDropEvent *event)
{
	const QMimeData *mimeData = event->mimeData();
	if(mimeData->hasImage()) {
		emit imageDropped(qvariant_cast<QImage>(event->mimeData()->imageData()));
	} else if(mimeData->hasUrls()) {
		emit urlDropped(event->mimeData()->urls().first());
	} else if(mimeData->hasColor()) {
		emit colorDropped(event->mimeData()->colorData().value<QColor>());
	} else {
		// unsupported data
		return;
	}
	event->acceptProposedAction();
}

void CanvasView::showEvent(QShowEvent *event)
{
	QGraphicsView::showEvent(event);
	// Find the DPI of the screen
	// TODO: if the window is moved to another screen, this should be updated
	QWidget *w = this;
	while(w) {
		if(w->windowHandle() != nullptr) {
			m_dpi = w->windowHandle()->screen()->physicalDotsPerInch();
			break;
		}
		w=w->parentWidget();
	}
}

void CanvasView::scrollContentsBy(int dx, int dy)
{
	QGraphicsView::scrollContentsBy(dx, dy);
	viewRectChanged();
}

void CanvasView::resizeEvent(QResizeEvent *e)
{
	QGraphicsView::resizeEvent(e);
	viewRectChanged();
}

}
