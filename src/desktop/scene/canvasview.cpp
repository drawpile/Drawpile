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

#include "config.h" // for KIS_TABLET
#include "canvasview.h"
#include "canvasscene.h"
#include "canvas/canvasmodel.h"
#include "drawdance/eventlog.h"

#include "notifications.h"

#include "widgets/notifbar.h"

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

// When KIS_TABLET isn't enabled (and maybe when the Qt version is new enough
// too, so I'm putting this into a separate #define), Qt will only generate
// mouse events when a tablet input goes unaccepted. We need those mouse events
// though, since it e.g. causes the cursor to update when the cursor enters the
// canvas or unfocuses the chat when you start drawing or panning with a tablet
// pen. So in that case, we don't accept those tablet events and instead add a
// check in our mouse event handlers to disregard synthetically generated mouse
// events, meaning we don't double up on them and the view acts properly.
#ifdef KIS_TABLET
#	undef PASS_PEN_EVENTS
#else
#	define PASS_PEN_EVENTS
#endif

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent), m_pendown(NOTDOWN),
	m_allowColorPick(false), m_allowToolAdjust(false), m_penmode(PenMode::Normal),
	m_dragmode(ViewDragMode::None), m_dragAction(CanvasShortcuts::NO_ACTION),
	m_dragByKey(true), m_dragInverted(false), m_dragSwapAxes(false),
	m_prevoutline(false), m_outlineSize(2), m_showoutline(true), m_subpixeloutline(true), m_squareoutline(false),
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
	m_brushOutlineWidth(1.0)
{
	viewport()->setAcceptDrops(true);
#ifdef Q_OS_MAC // Standard touch events seem to work better with mac touchpad
	viewport()->grabGesture(Qt::PinchGesture);
#else
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
#endif
	viewport()->setMouseTracking(true);
	setAcceptDrops(true);
	setFrameShape(QFrame::NoFrame);

	setBackgroundBrush(QColor(100,100,100));

	m_notificationBar = new NotificationBar(this);
	connect(m_notificationBar, &NotificationBar::actionButtonClicked, this, &CanvasView::reconnectRequested);

	m_trianglerightcursor = QCursor(QPixmap(":/cursors/triangle-right.png"), 14, 14);
	m_triangleleftcursor = QCursor(QPixmap(":/cursors/triangle-left.png"), 14, 14);
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

	updateSettings();
}

void CanvasView::showDisconnectedWarning(const QString &message)
{
	m_notificationBar->show(message, tr("Reconnect"), NotificationBar::RoleColor::Warning);
}

void CanvasView::updateSettings()
{
	QSettings cfg;
	cfg.beginGroup("settings/canvasshortcuts2");
	m_canvasShortcuts = CanvasShortcuts::load(cfg);
	cfg.endGroup();

	cfg.beginGroup("settings");
	Qt::ScrollBarPolicy policy = cfg.value("canvasscrollbars", false).toBool()
		? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
	setHorizontalScrollBarPolicy(policy);
	setVerticalScrollBarPolicy(policy);
	cfg.endGroup();
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

void CanvasView::setToolCapabilities(bool allowColorPick, bool allowToolAdjust)
{
	m_allowColorPick = allowColorPick;
	m_allowToolAdjust = allowToolAdjust;
}

void CanvasView::resetCursor()
{
	if(m_dragmode != ViewDragMode::None) {
		switch(m_dragAction) {
		case CanvasShortcuts::CANVAS_PAN:
			viewport()->setCursor(m_dragmode == ViewDragMode::Started
				? Qt::ClosedHandCursor : Qt::OpenHandCursor);
			break;
		case CanvasShortcuts::CANVAS_ROTATE:
			viewport()->setCursor(m_rotatecursor);
			break;
		case CanvasShortcuts::CANVAS_ZOOM:
			viewport()->setCursor(m_zoomcursor);
			break;
		case CanvasShortcuts::TOOL_ADJUST:
			viewport()->setCursor(
				m_dragSwapAxes ? Qt::SplitVCursor : Qt::SplitHCursor);
			break;
		default:
			viewport()->setCursor(Qt::ForbiddenCursor);
			break;
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
		case 1: viewport()->setCursor(Qt::CrossCursor); return;
		case 2: viewport()->setCursor(Qt::ArrowCursor); return;
		case 3: viewport()->setCursor(m_trianglerightcursor); return;
		case 4: viewport()->setCursor(m_triangleleftcursor); return;
		default: viewport()->setCursor(m_dotcursor); return;
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

	// We want to show an outline if we're currently drawing or able to, but
	// also if we're adjusting the tool size, since seeing the outline while
	// you change your brush size is really useful.
	bool outlineVisibleInMode;
	if(m_dragmode == ViewDragMode::None) {
		outlineVisibleInMode = m_penmode == PenMode::Normal && !m_locked;
	} else {
		outlineVisibleInMode = m_dragAction == CanvasShortcuts::TOOL_ADJUST;
	}

	bool shouldRenderOutline = m_showoutline && m_outlineSize > 0 && outlineVisibleInMode;
	m_prevoutline = shouldRenderOutline;
	if(shouldRenderOutline) {
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

void CanvasView::enterEvent(shim::EnterEvent *event)
{
	QGraphicsView::enterEvent(event);
	m_showoutline = true;

	// Give focus to this widget on mouseover. This is so that
	// using a key for dragging works rightaway. Avoid stealing
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
	m_prevoutline = false;
	updateOutline();
}

void CanvasView::focusInEvent(QFocusEvent *event)
{
	QGraphicsView::focusInEvent(event);
	m_keysDown.clear();
}

canvas::Point CanvasView::mapToScene(const QPoint &point, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation) const
{
	return canvas::Point(mapToScene(point), pressure, xtilt, ytilt, rotation);
}

canvas::Point CanvasView::mapToScene(const QPointF &point, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation) const
{
	return canvas::Point(mapToSceneInterpolate(point), pressure, xtilt, ytilt, rotation);
}

QPointF CanvasView::mapToSceneInterpolate(const QPointF &point) const
{
	// QGraphicsView API lacks mapToScene(QPointF), even though
	// the QPoint is converted to QPointF internally...
	// To work around this, map (x,y) and (x+1, y+1) and linearly interpolate
	// between the two
	double tmp;
	qreal xf = qAbs(modf(point.x(), &tmp));
	qreal yf = qAbs(modf(point.y(), &tmp));

	QPoint p0{qFloor(point.x()), qFloor(point.y())};
	QPointF p1 = mapToScene(p0);
	QPointF p2 = mapToScene(p0 + QPoint{1, 1});

	return QPointF{
		(p1.x()-p2.x()) * xf + p2.x(),
		(p1.y()-p2.y()) * yf + p2.y(),
	};
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
	if(!m_locked && m_penmode == PenMode::Normal) {
		emit penUp();
	}
}

void CanvasView::penPressEvent(const QPointF &pos, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButton button, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(m_pendown != NOTDOWN) {
		return;
	}

	CanvasShortcuts::Match match = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, button);
	PenMode penmode = PenMode::Normal;

	switch(match.action) {
	case CanvasShortcuts::NO_ACTION:
		break;
	case CanvasShortcuts::TOOL_ADJUST:
		if(!m_allowToolAdjust) {
			break;
		}
		Q_FALLTHROUGH();
	case CanvasShortcuts::CANVAS_PAN:
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ZOOM:
		if(m_dragmode != ViewDragMode::Started) {
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = match.action;
			m_dragInverted = match.inverted();
			m_dragSwapAxes = match.swapAxes();
		}
		break;
	case CanvasShortcuts::COLOR_PICK:
		if(m_allowColorPick) {
			penmode = PenMode::Colorpick;
		}
		break;
	case CanvasShortcuts::LAYER_PICK:
		penmode = PenMode::Layerpick;
		break;
	default:
		qWarning("Unhandled mouse button canvas shortcut %u", match.action);
		break;
	}

	if(m_dragmode == ViewDragMode::Prepared) {
		m_dragLastPoint = pos.toPoint();
		m_dragmode = ViewDragMode::Started;
		m_dragByKey = true;
		resetCursor();
		updateOutline();
	} else if((button == Qt::LeftButton || button == Qt::RightButton) && m_dragmode == ViewDragMode::None) {
		m_pendown = isStylus ? TABLETDOWN : MOUSEDOWN;
		m_pointerdistance = 0;
		m_pointervelocity = 0;
		m_prevpoint = mapToScene(pos, pressure, xtilt, ytilt, rotation);
		if(penmode != m_penmode) {
			m_penmode = penmode;
			resetCursor();
		}
		onPenDown(mapToScene(pos, mapPressure(pressure, isStylus), xtilt, ytilt, rotation), button == Qt::RightButton);
	}
}

static bool isSynthetic(QMouseEvent *event)
{
#ifdef PASS_PEN_EVENTS
	return event->source() & Qt::MouseEventSynthesizedByQt;
#else
	return false;
#endif
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	DP_EVENT_LOG(
		"mouse_press x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d pendown=%d touching=%d",
		event->x(), event->y(), unsigned(event->buttons()), unsigned(event->modifiers()),
		isSynthetic(event), m_pendown, m_touching);

	if(isSynthetic(event) || m_touching) {
		return;
	}

	penPressEvent(
		event->pos(),
		1.0,
		0.0,
		0.0,
		0.0,
		event->button(),
		event->modifiers(),
		false
	);
}

void CanvasView::penMoveEvent(const QPointF &pos, qreal pressure, qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(m_dragmode == ViewDragMode::Started) {
		moveDrag(pos.toPoint());

	} else {
		canvas::Point point = mapToScene(pos, pressure, xtilt, ytilt, rotation);
		updateOutline(point);
		if(!m_prevpoint.intSame(point)) {
			if(m_pendown) {
				m_pointervelocity = point.distance(m_prevpoint);
				m_pointerdistance += m_pointervelocity;
				point.setPressure(mapPressure(pressure, isStylus));
				CanvasShortcuts::Match match =
					m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
				onPenMove(
					point,
					buttons.testFlag(Qt::RightButton),
					match.toolConstraint1(),
					match.toolConstraint2());

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
	DP_EVENT_LOG(
		"mouse_move x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d pendown=%d touching=%d",
		event->x(), event->y(), unsigned(event->buttons()), unsigned(event->modifiers()),
		isSynthetic(event), m_pendown, m_touching);

	if(isSynthetic(event) || m_pendown == TABLETDOWN || m_touching) {
		return;
	}

	if(m_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	penMoveEvent(
		event->pos(),
		1.0,
		0.0,
		0.0,
		0.0,
		event->buttons(),
		event->modifiers(),
		false
	);
}

void CanvasView::penReleaseEvent(const QPointF &pos, Qt::MouseButton button, Qt::KeyboardModifiers modifiers)
{
	canvas::Point point = mapToScene(pos, 0.0, 0.0, 0.0, 0.0);
	m_prevpoint = point;
	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, Qt::LeftButton);

	if(m_dragmode != ViewDragMode::None) {
		switch(mouseMatch.action) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
		default:
			m_dragmode = ViewDragMode::None;
			break;
		}

	} else if(m_pendown == TABLETDOWN || ((button == Qt::LeftButton || button == Qt::RightButton) && m_pendown == MOUSEDOWN)) {
		onPenUp();
		m_pendown = NOTDOWN;
		switch(mouseMatch.action) {
		case CanvasShortcuts::TOOL_ADJUST:
			if(!m_allowToolAdjust) {
				m_penmode = PenMode::Normal;
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
			m_penmode = PenMode::Normal;
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
		case CanvasShortcuts::COLOR_PICK:
			if(m_allowColorPick) {
				m_penmode = PenMode::Colorpick;
			}
			break;
		case CanvasShortcuts::LAYER_PICK:
			m_penmode = PenMode::Layerpick;
			break;
		default:
			m_penmode = PenMode::Normal;
			break;
		}
	}

	resetCursor();
	updateOutline(point);
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	DP_EVENT_LOG(
		"mouse_release x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d pendown=%d touching=%d",
		event->x(), event->y(), unsigned(event->buttons()), unsigned(event->modifiers()),
		isSynthetic(event), m_pendown, m_touching);
	if(isSynthetic(event) || m_touching) {
		return;
	}
	penReleaseEvent(event->pos(), event->button(), event->modifiers());
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent*)
{
	// Ignore doubleclicks
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	CanvasShortcuts::Match match = m_canvasShortcuts.matchMouseWheel(
		event->modifiers(), m_keysDown);
	QPoint angleDelta = event->angleDelta();
	int deltaX = angleDelta.x();
	int deltaY = angleDelta.y();
	if(match.inverted()) {
		deltaX = -deltaX;
		deltaY = -deltaY;
	}
	if(match.swapAxes()) {
		std::swap(deltaX, deltaY);
	}

	switch(match.action) {
	case CanvasShortcuts::NO_ACTION:
		break;
	case CanvasShortcuts::CANVAS_PAN:
		scrollBy(-deltaX, -deltaY);
		break;
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ZOOM: {
		event->accept();
		m_zoomWheelDelta += deltaY;
		int steps = m_zoomWheelDelta / 120;
		m_zoomWheelDelta -= steps * 120;
		if(steps != 0) {
			if(match.action == CanvasShortcuts::CANVAS_ROTATE) {
				setRotation(rotation() + steps * 10);
			} else {
				zoomSteps(steps);
			}
		}
		break;
	}
	// Color and layer picking by spinning the scroll wheel is weird, but okay.
	case CanvasShortcuts::COLOR_PICK:
		if(m_allowColorPick && m_scene->hasImage()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
			QPointF p = mapToSceneInterpolate(mapFromGlobal(QCursor::pos()));
#else
			QPointF p = mapToSceneInterpolate(event->position());
#endif
			m_scene->model()->pickColor(p.x(), p.y(), 0, 0);
		}
		break;
	case CanvasShortcuts::LAYER_PICK: {
		if(m_scene->hasImage()) {
#if QT_VERSION < QT_VERSION_CHECK(5, 14, 0)
			QPointF p = mapToSceneInterpolate(mapFromGlobal(QCursor::pos()));
#else
			QPointF p = mapToSceneInterpolate(event->position());
#endif
			m_scene->model()->pickLayer(p.x(), p.y());
		}
		break;
	}
	case CanvasShortcuts::TOOL_ADJUST:
		if(m_allowToolAdjust) {
			event->accept();
			emit quickAdjust(deltaY / (30 * 4.0));
		}
		break;
	default:
		qWarning("Unhandled mouse wheel canvas shortcut %u", match.action);
		break;
	}
}

void CanvasView::keyPressEvent(QKeyEvent *event) {
	QGraphicsView::keyPressEvent(event);
	if(event->isAutoRepeat()) {
		return;
	}

	if(m_pendown == NOTDOWN) {
		CanvasShortcuts::Match keyMatch = m_canvasShortcuts.matchKeyCombination(
			event->modifiers(), Qt::Key(event->key()));
		switch(keyMatch.action) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_penmode = PenMode::Normal;
			m_dragmode = ViewDragMode::Started;
			m_dragAction = keyMatch.action;
			m_dragByKey = true;
			m_dragInverted = keyMatch.inverted();
			m_dragSwapAxes = keyMatch.swapAxes();
			m_dragLastPoint = mapFromGlobal(QCursor::pos());
			resetCursor();
			updateOutline();
			break;
		default:
			break;
		}
	} else {
		QGraphicsView::keyPressEvent(event);
	}

	m_keysDown.insert(Qt::Key(event->key()));

	if(m_pendown == NOTDOWN) {
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			event->modifiers(), m_keysDown, Qt::LeftButton);
		switch(mouseMatch.action) {
		case CanvasShortcuts::TOOL_ADJUST:
			if(!m_allowToolAdjust) {
				m_penmode = PenMode::Normal;
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
			m_penmode = PenMode::Normal;
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			updateOutline();
			break;
		case CanvasShortcuts::COLOR_PICK:
			m_penmode = m_allowColorPick ? PenMode::Colorpick : PenMode::Normal;
			break;
		case CanvasShortcuts::LAYER_PICK:
			m_penmode = PenMode::Layerpick;
			break;
		default:
			m_penmode = PenMode::Normal;
			break;
		}
	}

	resetCursor();
	updateOutline();
}

void CanvasView::keyReleaseEvent(QKeyEvent *event) {
	QGraphicsView::keyReleaseEvent(event);
	if(event->isAutoRepeat()) {
		return;
	}

	m_keysDown.remove(Qt::Key(event->key()));

	if(m_dragmode == ViewDragMode::Started && m_dragByKey) {
		CanvasShortcuts::Match keyMatch = m_canvasShortcuts.matchKeyCombination(
			event->modifiers(), Qt::Key(event->key()));
		switch(keyMatch.action) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragmode = ViewDragMode::None;
			resetCursor();
			updateOutline();
			break;
		default:
			break;
		}
	}

	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		event->modifiers(), m_keysDown, Qt::LeftButton);
	if(m_dragmode == ViewDragMode::Prepared) {
		switch(mouseMatch.action) {
		case CanvasShortcuts::TOOL_ADJUST:
			if(!m_allowToolAdjust) {
				m_dragmode = ViewDragMode::None;
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			updateOutline();
			break;
		default:
			m_dragmode = ViewDragMode::None;
			break;
		}
	}

	if(m_pendown == NOTDOWN) {
		switch(mouseMatch.action) {
		case CanvasShortcuts::COLOR_PICK:
			m_penmode = m_allowColorPick ? PenMode::Colorpick : PenMode::Normal;
			break;
		case CanvasShortcuts::LAYER_PICK:
			m_penmode = PenMode::Layerpick;
			break;
		default:
			m_penmode = PenMode::Normal;
			break;
		}
	}

	resetCursor();
	updateOutline();
}

void CanvasView::gestureEvent(QGestureEvent *event)
{
	QPinchGesture *pinch = static_cast<QPinchGesture*>(event->gesture(Qt::PinchGesture));
	if(pinch) {
		DP_EVENT_LOG(
			"pinch change=0x%x scale=%f rotation=%f pendown=%d touching=%d",
			unsigned(pinch->changeFlags()), pinch->totalScaleFactor(),
			pinch->totalRotationAngle(), m_pendown, m_touching);

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
		DP_EVENT_LOG("touch_begin pendown=%d touching=%d", m_pendown, m_touching);
		m_touchRotating = false;
		break;

	case QEvent::TouchUpdate: {
		QPointF startCenter, lastCenter, center;
		const int points = event->touchPoints().size();
		for(const auto &tp : event->touchPoints()) {
			startCenter += tp.startPos();
			lastCenter += tp.lastPos();
			center += tp.pos();
		}
		startCenter /= points;
		lastCenter /= points;
		center /= points;

		DP_EVENT_LOG(
			"touch_update points=%d x=%f y=%f pendown=%d touching=%d",
			points, center.x(), center.y(), m_pendown, m_touching);

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
			for(const auto &tp : event->touchPoints()) {
				startAvgDist += squareDist(tp.startPos() - startCenter);
				avgDist += squareDist(tp.pos() - center);
			}
			startAvgDist = sqrt(startAvgDist);

			if(m_enableTouchPinch) {
				avgDist = sqrt(avgDist);
				const qreal dZoom = avgDist / startAvgDist;
				m_zoom = m_touchStartZoom * dZoom;
			}

			if(m_enableTouchTwist) {
				const QLineF l1 { event->touchPoints().first().startPos(), event->touchPoints().last().startPos() };
				const QLineF l2 { event->touchPoints().first().pos(), event->touchPoints().last().pos() };

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
		DP_EVENT_LOG("touch_end pendown=%d touching=%d", m_pendown, m_touching);
		m_touching = false;
		break;

	case QEvent::TouchCancel:
		DP_EVENT_LOG("touch_cancel pendown=%d touching=%d", m_pendown, m_touching);
		m_touching = false;
		break;

	default:
		break;
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
#ifndef Q_OS_MAC // On Mac, the above gesture events work better
	else if(event->type()==QEvent::TouchBegin || event->type() == QEvent::TouchUpdate || event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
		touchEvent(static_cast<QTouchEvent*>(event));
	}
#endif
	else if(event->type() == QEvent::TabletPress && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		DP_EVENT_LOG(
			"tablet_press x=%d y=%d pressure=%f xtilt=%f ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x pendown=%d touching=%d",
			tabev->x(), tabev->y(), tabev->pressure(), double(tabev->xTilt()),
			double(tabev->yTilt()), tabev->rotation(), unsigned(tabev->buttons()),
			unsigned(tabev->modifiers()), m_pendown, m_touching);

		// Note: it is possible to get a mouse press event for a tablet event (even before
		// the tablet event is received or even though tabev->accept() is called), but
		// it is never possible to get a TabletPress for a real mouse press. Therefore,
		// we don't actually do anything yet in the penDown handler other than remember
		// the initial point and we'll let a TabletEvent override the mouse event.
		// When KIS_TABLET isn't enabled we ignore synthetic mouse events though.
#ifndef PASS_PEN_EVENTS
		tabev->accept();
#endif

		penPressEvent(
			tabev->posF(),
			tabev->pressure(),
			tabev->xTilt(),
			tabev->yTilt(),
			tabev->rotation(),
			tabev->button(),
			QApplication::queryKeyboardModifiers(), // TODO check if tablet event modifiers() is still broken in Qt 5.12
			true
		);
	}
	else if(event->type() == QEvent::TabletMove && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		DP_EVENT_LOG(
			"tablet_move x=%d y=%d pressure=%f xtilt=%f ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x pendown=%d touching=%d",
			tabev->x(), tabev->y(), tabev->pressure(), double(tabev->xTilt()),
			double(tabev->yTilt()), tabev->rotation(), unsigned(tabev->buttons()),
			unsigned(tabev->modifiers()), m_pendown, m_touching);

#ifndef PASS_PEN_EVENTS
		tabev->accept();
#endif

		penMoveEvent(
			tabev->posF(),
			tabev->pressure(),
			tabev->xTilt(),
			tabev->yTilt(),
			tabev->rotation(),
			tabev->buttons(),
			QApplication::queryKeyboardModifiers(), // TODO check if tablet event modifiers() is still broken in Qt 5.12
			true
		);
	}
	else if(event->type() == QEvent::TabletRelease && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent*>(event);
		DP_EVENT_LOG(
			"tablet_release x=%d y=%d buttons=0x%x pendown=%d touching=%d",
			tabev->x(), tabev->y(), unsigned(tabev->buttons()),
			m_pendown, m_touching);
#ifndef PASS_PEN_EVENTS
		tabev->accept();
#endif
		// TODO check if tablet event modifiers() is still broken in Qt 5.12
		penReleaseEvent(tabev->posF(), tabev->button(), QApplication::queryKeyboardModifiers());
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
	if(m_showoutline && !m_locked && (!m_prevoutline || !point.roughlySame(m_prevoutlinepoint))) {
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
void CanvasView::moveDrag(const QPoint &point)
{
	int deltaX = m_dragLastPoint.x() - point.x();
	int deltaY = m_dragLastPoint.y() - point.y();
	if(m_dragInverted) {
		deltaX = -deltaX;
		deltaY = -deltaY;
	}
	if(m_dragSwapAxes) {
		std::swap(deltaX, deltaY);
	}

	switch(m_dragAction) {
	case CanvasShortcuts::CANVAS_PAN:
		scrollBy(deltaX, deltaY);
		break;
	case CanvasShortcuts::CANVAS_ROTATE: {
		qreal hw = width() / 2.0;
		qreal hh = height() / 2.0;
		qreal a1 = qAtan2(hw - m_dragLastPoint.x(), hh - m_dragLastPoint.y());
		qreal a2 = qAtan2(hw - point.x(), hh - point.y());
		qreal ad = qRadiansToDegrees(a1 - a2);
		setRotation(rotation() + (m_dragInverted ? -ad : ad));
		break;
	}
	case CanvasShortcuts::CANVAS_ZOOM:
		if(deltaY != 0) {
			qreal delta = qBound(-1.0, deltaY / 100.0, 1.0);
			if(delta > 0.0) {
				setZoom(m_zoom * (1.0 + delta));
			} else if(delta < 0.0) {
				setZoom(m_zoom / (1.0 - delta));
			}
		}
		break;
	case CanvasShortcuts::TOOL_ADJUST:
		// Horizontally, dragging right (+X) is higher and left (-X) is lower,
		// but vertically, dragging up (-Y) is higher and down (+Y) is lower.
		// We have to invert in one of those cases to match with that logic.
		if(!m_dragSwapAxes) {
			deltaX = -deltaX;
		}
		emit quickAdjust(qBound(-2.0, deltaX / 10.0, 2.0));
		break;
	default:
		qWarning("Unhandled drag action %u", m_dragAction);
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
	const QMimeData *data = event->mimeData();
	if(data->hasImage()) {
		emit imageDropped(qvariant_cast<QImage>(event->mimeData()->imageData()));
	} else if(data->hasUrls()) {
		emit urlDropped(event->mimeData()->urls().first());
	} else if(data->hasColor()) {
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
