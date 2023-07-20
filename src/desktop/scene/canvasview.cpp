// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/scene/canvasview.h"
#include "desktop/main.h"
#include "desktop/scene/canvasscene.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/drawdance/eventlog.h"
#include "libshared/util/qtcompat.h"

#include "desktop/tabletinput.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/widgets/notifbar.h"

#include <QApplication>
#include <QDateTime>
#include <QLineF>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QScopedValueRollback>
#include <QScreen>
#include <QScrollBar>
#include <QTabletEvent>
#include <QWindow>
#include <QtMath>
#include <cmath>

using libclient::settings::zoomMax;
using libclient::settings::zoomMin;

namespace widgets {

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent)
	, m_pendown(NOTDOWN)
	, m_allowColorPick(false)
	, m_allowToolAdjust(false)
	, m_toolHandlesRightClick(false)
	, m_penmode(PenMode::Normal)
	, m_dragmode(ViewDragMode::None)
	, m_dragAction(CanvasShortcuts::NO_ACTION)
	, m_dragButton(Qt::NoButton)
	, m_dragInverted(false)
	, m_dragSwapAxes(false)
	, m_prevoutline(false)
	, m_outlineSize(2)
	, m_showoutline(false)
	, m_subpixeloutline(true)
	, m_squareoutline(false)
	, m_pos{0.0, 0.0}
	, m_zoom(1.0)
	, m_rotate(0)
	, m_flip(false)
	, m_mirror(false)
	, m_scene(nullptr)
	, m_zoomWheelDelta(0)
	, m_enableTablet(true)
	, m_lock{Lock::None}
	, m_busy(false)
	, m_pointertracking(false)
	, m_pixelgrid(true)
	, m_enableTouchScroll(true)
	, m_enableTouchDraw(false)
	, m_enableTouchPinch(true)
	, m_enableTouchTwist(true)
	, m_touching(false)
	, m_touchRotating(false)
	, m_touchMode(TouchMode::Unknown)
	, m_dpi(96)
	, m_brushCursorStyle(BrushCursor::Dot)
	, m_brushOutlineWidth(1.0)
	, m_scrollBarsAdjusting{false}
	, m_blockNotices{false}
{
	viewport()->setAcceptDrops(true);
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
	viewport()->setMouseTracking(true);
	setAcceptDrops(true);
	setFrameShape(QFrame::NoFrame);

	setBackgroundBrush(QColor(100, 100, 100));

	m_notificationBar = new NotificationBar(this);
	connect(
		m_notificationBar, &NotificationBar::actionButtonClicked, this,
		&CanvasView::reconnectRequested);

	m_trianglerightcursor =
		QCursor(QPixmap(":/cursors/triangle-right.png"), 14, 14);
	m_triangleleftcursor =
		QCursor(QPixmap(":/cursors/triangle-left.png"), 14, 14);
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

	auto &settings = dpApp().settings();

	settings.bindTabletEvents(this, &widgets::CanvasView::setTabletEnabled);
	settings.bindOneFingerDraw(this, &widgets::CanvasView::setTouchDraw);
	settings.bindOneFingerScroll(this, &widgets::CanvasView::setTouchScroll);
	settings.bindTwoFingerZoom(this, &widgets::CanvasView::setTouchPinch);
	settings.bindTwoFingerRotate(this, &widgets::CanvasView::setTouchTwist);
	settings.bindBrushCursor(this, &widgets::CanvasView::setBrushCursorStyle);
	settings.bindBrushOutlineWidth(
		this, &widgets::CanvasView::setBrushOutlineWidth);

	settings.bindGlobalPressureCurve(this, [=](QString serializedCurve) {
		KisCubicCurve curve;
		curve.fromString(serializedCurve);
		setPressureCurve(curve);
	});

	settings.bindCanvasShortcuts(
		this, [=](desktop::settings::Settings::CanvasShortcutsType shortcuts) {
			m_canvasShortcuts = CanvasShortcuts::load(shortcuts);
		});

	settings.bindCanvasScrollBars(this, [=](bool enabled) {
		const auto policy =
			enabled ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff;
		setHorizontalScrollBarPolicy(policy);
		setVerticalScrollBarPolicy(policy);
	});
}

void CanvasView::showDisconnectedWarning(const QString &message)
{
	m_notificationBar->show(
		message, tr("Reconnect"), NotificationBar::RoleColor::Warning);
}

QString CanvasView::lockDescription() const
{
	QStringList reasons;

	if(m_lock.testFlag(Lock::Reset)) {
		reasons.append(tr("Reset in progress"));
	}

	if(m_lock.testFlag(Lock::Canvas)) {
		reasons.append(tr("Canvas is locked"));
	}

	if(m_lock.testFlag(Lock::User)) {
		reasons.append(tr("User is locked"));
	}

	if(m_lock.testFlag(Lock::LayerGroup)) {
		reasons.append(tr("Layer is a group"));
	} else if(m_lock.testFlag(Lock::LayerLocked)) {
		reasons.append(tr("Layer is locked"));
	} else if(m_lock.testFlag(Lock::LayerCensored)) {
		reasons.append(tr("Layer is censored"));
	} else if(m_lock.testFlag(Lock::LayerHidden)) {
		reasons.append(tr("Layer is hidden"));
	}

	if(m_lock.testFlag(Lock::Tool)) {
		reasons.append(tr("Tool is locked"));
	}

	return reasons.join(QStringLiteral("\n"));
}

void CanvasView::setCanvas(drawingboard::CanvasScene *scene)
{
	m_scene = scene;
	setScene(scene);
	connect(
		m_scene, &drawingboard::CanvasScene::canvasResized, this,
		[this](int xoff, int yoff, const QSize &oldsize) {
			if(oldsize.isEmpty()) {
				zoomToFit();
			} else {
				scrollBy(xoff * m_zoom, yoff * m_zoom);
			}
			viewRectChanged();
		});

	viewRectChanged();
	updateLockNotice();
}

void CanvasView::scrollBy(int x, int y)
{
	updateCanvasTransform([&] {
		m_pos.setX(m_pos.x() + x);
		m_pos.setY(m_pos.y() + y);
	});
}

void CanvasView::zoomSteps(int steps)
{
	zoomStepsAt(steps, mapToCanvas(rect().center()));
}

void CanvasView::zoomStepsAt(int steps, const QPointF &point)
{
	constexpr qreal eps = 1e-5;
	const QVector<qreal> &zoomLevels = libclient::settings::zoomLevels();
	// This doesn't actually take the number of steps into account, it just
	// zooms by a single step. But that works really well, so I'll leave it be.
	if(steps > 0) {
		int i = 0;
		while(i < zoomLevels.size() - 1 && m_zoom > zoomLevels[i] - eps) {
			i++;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom > level - eps ? zoomMax : qMax(m_zoom, level);
		setZoomAt(zoom, point);
	} else if(steps < 0) {
		int i = zoomLevels.size() - 1;
		while(i > 0 && m_zoom < zoomLevels[i] + eps) {
			i--;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom < level + eps ? zoomMin : qMin(m_zoom, level);
		setZoomAt(zoom, point);
	}
}

void CanvasView::moveStep(Direction direction)
{
	switch(direction) {
	case Direction::Left:
		scrollBy(-horizontalScrollBar()->singleStep(), 0);
		break;
	case Direction::Right:
		scrollBy(horizontalScrollBar()->singleStep(), 0);
		break;
	case Direction::Up:
		scrollBy(0, -verticalScrollBar()->singleStep());
		break;
	case Direction::Down:
		scrollBy(0, verticalScrollBar()->singleStep());
		break;
	}
}

void CanvasView::zoomin()
{
	zoomStepsAt(1, mapToCanvas(rect().center()));
}

void CanvasView::zoomout()
{
	zoomStepsAt(-1, mapToCanvas(rect().center()));
}

void CanvasView::zoomTo(const QRect &rect, int steps)
{
	if(rect.width() < 15 || rect.height() < 15 || steps < 0) {
		zoomStepsAt(steps, rect.center());
	} else {
		QWidget *vp = viewport();
		QRectF viewRect = mapFromCanvas(rect).boundingRect();
		qreal xScale = qreal(vp->width()) / viewRect.width();
		qreal yScale = qreal(vp->height()) / viewRect.height();
		setZoomAt(m_zoom * qMin(xScale, yScale), rect.center());
	}
}

void CanvasView::zoomToFit()
{
	setZoomToFit(Qt::Horizontal | Qt::Vertical);
}

void CanvasView::zoomToFitWidth()
{
	setZoomToFit(Qt::Horizontal);
}

void CanvasView::zoomToFitHeight()
{
	setZoomToFit(Qt::Vertical);
}

void CanvasView::setZoomToFit(Qt::Orientations orientations)
{
	if(m_scene && m_scene->hasImage()) {
		QWidget *vp = viewport();
		QRectF r{QPointF{}, QSizeF{m_scene->model()->size()}};
		qreal xScale = qreal(vp->width()) / r.width();
		qreal yScale = qreal(vp->height()) / r.height();
		qreal scale;
		if(orientations.testFlag(Qt::Horizontal)) {
			if(orientations.testFlag(Qt::Vertical)) {
				scale = qMin(xScale, yScale);
			} else {
				scale = xScale;
			}
		} else {
			scale = yScale;
		}

		qreal rotate = m_rotate;
		bool mirror = m_mirror;
		bool flip = m_flip;

		m_pos = r.center();
		m_zoom = 1.0;
		m_rotate = 0.0;
		m_mirror = false;
		m_flip = false;

		QScopedValueRollback<bool> guard{m_blockNotices, true};
		setZoomAt(scale, m_pos);
		setRotation(rotate);
		setViewMirror(mirror);
		setViewFlip(flip);
	}
}

void CanvasView::setZoom(qreal zoom)
{
	setZoomAt(zoom, mapToCanvas(rect().center()));
}

void CanvasView::setZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(zoomMin, zoom, zoomMax);
	if(newZoom != m_zoom) {
		QTransform matrix;
		mirrorFlip(matrix, m_mirror, m_flip);
		matrix.rotate(m_rotate);

		updateCanvasTransform([&] {
			m_pos += matrix.map(point * (newZoom - m_zoom));
			m_zoom = newZoom;
		});

		emitViewTransformed();
		showTransformNotice(getZoomNotice());
	}
}

void CanvasView::setRotation(qreal angle)
{
	angle = std::fmod(angle, 360.0);
	if(angle < 0.0) {
		angle += 360.0;
	}

	bool inverted = isRotationInverted();
	if(inverted) {
		angle = 360.0 - angle;
	}

	if(angle != m_rotate) {
		QTransform prev, cur;
		prev.rotate(m_rotate);
		cur.rotate(angle);

		updateCanvasTransform([&] {
			if(inverted) {
				m_pos = cur.inverted().map(prev.map(m_pos));
			} else {
				m_pos = prev.inverted().map(cur.map(m_pos));
			}
			m_rotate = angle;
		});

		emitViewTransformed();
		showTransformNotice(getRotationNotice());
	}
}

void CanvasView::setViewFlip(bool flip)
{
	if(flip != m_flip) {
		QTransform prev, cur;
		mirrorFlip(prev, m_mirror, m_flip);
		mirrorFlip(cur, m_mirror, flip);

		updateCanvasTransform([&] {
			m_pos = cur.inverted().map(prev.map(m_pos));
			m_flip = flip;
		});

		emitViewTransformed();
		showTransformNotice(
			m_flip ? tr("Vertical flip: ON") : tr("Vertical flip: OFF"));
	}
}

void CanvasView::setViewMirror(bool mirror)
{
	if(mirror != m_mirror) {
		QTransform prev, cur;
		mirrorFlip(prev, m_mirror, m_flip);
		mirrorFlip(cur, mirror, m_flip);

		updateCanvasTransform([&] {
			m_pos = cur.inverted().map(prev.map(m_pos));
			m_mirror = mirror;
		});

		emitViewTransformed();
		showTransformNotice(
			m_mirror ? tr("Horizontal mirror: ON")
					 : tr("Horizontal mirror: OFF"));
	}
}

void CanvasView::setLock(QFlags<Lock> lock)
{
	m_lock = lock;
	resetCursor();
	updateLockNotice();
}

void CanvasView::setBusy(bool busy)
{
	m_busy = busy;
	resetCursor();
}

void CanvasView::setBrushOutlineWidth(qreal outlineWidth)
{
	m_brushOutlineWidth = qIsNaN(outlineWidth) ? 1.0 : outlineWidth;
	resetCursor();
}

void CanvasView::setToolCursor(const QCursor &cursor)
{
	m_toolcursor = cursor;
	resetCursor();
}

void CanvasView::setToolCapabilities(
	bool allowColorPick, bool allowToolAdjust, bool toolHandlesRightClick)
{
	m_allowColorPick = allowColorPick;
	m_allowToolAdjust = allowToolAdjust;
	m_toolHandlesRightClick = toolHandlesRightClick;
}

void CanvasView::resetCursor()
{
	if(m_dragmode != ViewDragMode::None) {
		switch(m_dragAction) {
		case CanvasShortcuts::CANVAS_PAN:
			viewport()->setCursor(
				m_dragmode == ViewDragMode::Started ? Qt::ClosedHandCursor
													: Qt::OpenHandCursor);
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
	case PenMode::Normal:
		break;
	case PenMode::Colorpick:
		viewport()->setCursor(m_colorpickcursor);
		return;
	case PenMode::Layerpick:
		viewport()->setCursor(m_layerpickcursor);
		return;
	}

	if(m_lock) {
		viewport()->setCursor(Qt::ForbiddenCursor);
		updateOutline();
		return;
	} else if(m_busy) {
		viewport()->setCursor(Qt::WaitCursor);
		updateOutline();
		return;
	}

	if(m_toolcursor.shape() == Qt::CrossCursor) {
		switch(m_brushCursorStyle) {
		case BrushCursor::Dot:
			viewport()->setCursor(m_dotcursor);
			break;
		case BrushCursor::Cross:
			viewport()->setCursor(Qt::CrossCursor);
			break;
		case BrushCursor::Arrow:
			viewport()->setCursor(Qt::ArrowCursor);
			break;
		case BrushCursor::TriangleLeft:
			viewport()->setCursor(m_triangleleftcursor);
			break;
		case BrushCursor::TriangleRight:
		default:
			viewport()->setCursor(m_trianglerightcursor);
			break;
		}
	} else {
		viewport()->setCursor(m_toolcursor);
	}
}

void CanvasView::setPixelGrid(bool enable)
{
	m_pixelgrid = enable;
	updateCanvasPixelGrid();
}

/**
 * @param radius circle radius
 */
void CanvasView::setOutlineSize(int newSize)
{
	if(m_showoutline && (m_outlineSize > 0 || newSize > 0)) {
		updateScene({getOutlineBounds(
			m_prevoutlinepoint, qMax(m_outlineSize, newSize))});
	}
	m_outlineSize = newSize;
}

void CanvasView::setOutlineMode(bool subpixel, bool square)
{
	m_subpixeloutline = subpixel;
	m_squareoutline = square;
}

void CanvasView::drawForeground(QPainter *painter, const QRectF &rect)
{
	// We want to show an outline if we're currently drawing or able to, but
	// also if we're adjusting the tool size, since seeing the outline while
	// you change your brush size is really useful.
	bool outlineVisibleInMode;
	if(m_dragmode == ViewDragMode::None) {
		outlineVisibleInMode =
			m_penmode == PenMode::Normal && !m_lock && !m_busy;
	} else {
		outlineVisibleInMode = m_dragAction == CanvasShortcuts::TOOL_ADJUST;
	}

	bool shouldRenderOutline =
		m_showoutline && m_outlineSize > 0 && outlineVisibleInMode;
	m_prevoutline = shouldRenderOutline;
	if(shouldRenderOutline) {
		qreal size = m_outlineSize * m_zoom;
		QRectF outline(
			mapFromCanvas(m_prevoutlinepoint) - QPointF(size / 2.0, size / 2.0),
			QSizeF(size, size));

		if(!m_subpixeloutline && m_outlineSize % 2 == 0) {
			outline.translate(-0.5 * m_zoom, -0.5 * m_zoom);
		}

		if(rect.intersects(outline) && m_brushOutlineWidth > 0) {
			painter->save();
			QPen pen(QColor(96, 191, 96));
			pen.setCosmetic(true);
			pen.setWidthF(m_brushOutlineWidth);
			painter->setPen(pen);
			painter->setCompositionMode(
				QPainter::RasterOp_SourceXorDestination);
			if(m_squareoutline)
				painter->drawRect(outline);
			else
				painter->drawEllipse(outline);
			painter->restore();
		}
	}
}

void CanvasView::viewRectChanged()
{
	if(m_scene) {
		m_scene->setSceneBounds(mapToScene(viewport()->rect()).boundingRect());
	}
	emit viewRectChange(mapToCanvas(rect()));
}

void CanvasView::enterEvent(compat::EnterEvent *event)
{
	QGraphicsView::enterEvent(event);
	m_showoutline = true;

	// Give focus to this widget on mouseover. This is so that
	// using a key for dragging works rightaway. Avoid stealing
	// focus from text edit widgets though.
	QWidget *oldfocus = QApplication::focusWidget();
	if(!oldfocus ||
	   !(oldfocus->inherits("QLineEdit") || oldfocus->inherits("QTextEdit") ||
		 oldfocus->inherits("QPlainTextEdit"))) {
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

canvas::Point CanvasView::mapToCanvas(
	long long timeMsec, const QPoint &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation) const
{
	return canvas::Point(
		timeMsec, mapToCanvas(point), m_pressureCurve.value(pressure), xtilt,
		ytilt, rotation);
}

canvas::Point CanvasView::mapToCanvas(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation) const
{
	return canvas::Point(
		timeMsec, mapToCanvas(point), m_pressureCurve.value(pressure), xtilt,
		ytilt, rotation);
}

QPointF CanvasView::mapToCanvas(const QPoint &point) const
{
	return mapToCanvas(QPointF{point});
}

QPointF CanvasView::mapToCanvas(const QPointF &point) const
{
	QTransform matrix = toCanvasTransform();
	return matrix.map(point + mapToScene(QPoint{0, 0}));
}

QPolygonF CanvasView::mapToCanvas(const QRect &rect) const
{
	return mapToCanvas(QRectF{rect});
}

QPolygonF CanvasView::mapToCanvas(const QRectF &rect) const
{
	if(rect.isValid()) {
		QTransform matrix = toCanvasTransform();
		QPointF offset = mapToScene(QPoint{0, 0});
		return matrix.map(rect.translated(offset));
	} else {
		return QPolygonF{};
	}
}

QTransform CanvasView::fromCanvasTransform() const
{
	return m_scene ? m_scene->canvasTransform() : QTransform{};
}

QTransform CanvasView::toCanvasTransform() const
{
	return m_scene ? m_scene->canvasTransform().inverted() : QTransform{};
}

QPointF CanvasView::mapFromCanvas(const QPointF &point) const
{
	return fromCanvasTransform().map(point);
}

QPolygonF CanvasView::mapFromCanvas(const QRect &rect) const
{
	if(rect.isValid()) {
		return fromCanvasTransform().mapToPolygon(rect);
	} else {
		return QPolygonF{};
	}
}

void CanvasView::setPointerTracking(bool tracking)
{
	m_pointertracking = tracking;
	if(!tracking && m_scene) {
		// TODO
		//_scene->hideUserMarker();
	}
}

void CanvasView::onPenDown(const canvas::Point &p, bool right)
{
	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_lock)
				emit penDown(
					p.timeMsec(), p, p.pressure(), p.xtilt(), p.ytilt(),
					p.rotation(), right, m_zoom);
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

void CanvasView::onPenMove(
	const canvas::Point &p, bool right, bool constrain1, bool constrain2)
{
	Q_UNUSED(right)

	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_lock)
				emit penMove(
					p.timeMsec(), p, p.pressure(), p.xtilt(), p.ytilt(),
					p.rotation(), constrain1, constrain2);
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
	if(!m_lock && m_penmode == PenMode::Normal) {
		emit penUp();
	}
}

void CanvasView::penPressEvent(
	long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers, bool isStylus)
{
	if(m_pendown != NOTDOWN) {
		return;
	}

	CanvasShortcuts::Match match =
		m_canvasShortcuts.matchMouseButton(modifiers, m_keysDown, button);
	// If the tool wants to receive right clicks (e.g. to undo the last point in
	// a bezier curve), we have to override that shortcut.
	if(m_toolHandlesRightClick && match.isUnmodifiedClick(Qt::RightButton)) {
		match.shortcut = nullptr;
	}

	PenMode penmode = PenMode::Normal;

	switch(match.action()) {
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
			m_dragAction = match.action();
			m_dragButton = match.shortcut->button;
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
		qWarning("Unhandled mouse button canvas shortcut %u", match.action());
		break;
	}

	if(m_dragmode == ViewDragMode::Prepared) {
		m_dragLastPoint = pos.toPoint();
		m_dragmode = ViewDragMode::Started;
		m_dragButton = button;
		resetCursor();
		updateOutline();
	} else if(
		(button == Qt::LeftButton || button == Qt::RightButton) &&
		m_dragmode == ViewDragMode::None) {
		m_pendown = isStylus ? TABLETDOWN : MOUSEDOWN;
		m_pointerdistance = 0;
		m_pointervelocity = 0;
		m_prevpoint =
			mapToCanvas(timeMsec, pos, pressure, xtilt, ytilt, rotation);
		if(penmode != m_penmode) {
			m_penmode = penmode;
			resetCursor();
		}
		onPenDown(
			mapToCanvas(timeMsec, pos, pressure, xtilt, ytilt, rotation),
			button == Qt::RightButton);
	}
}

static bool isSynthetic(QMouseEvent *event)
{
	if(tabletinput::passPenEvents()) {
		return event->source() & Qt::MouseEventSynthesizedByQt;
	} else {
		return false;
	}
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	const auto mousePos = compat::mousePos(*event);
	DP_EVENT_LOG(
		"mouse_press x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d "
		"pendown=%d touching=%d",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), isSynthetic(event), m_pendown,
		m_touching);

	if(isSynthetic(event) || m_touching) {
		return;
	}

	penPressEvent(
		QDateTime::currentMSecsSinceEpoch(), mousePos, 1.0, 0.0, 0.0, 0.0,
		event->button(), event->modifiers(), false);
}

void CanvasView::penMoveEvent(
	long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::MouseButtons buttons,
	Qt::KeyboardModifiers modifiers)
{
	if(m_dragmode == ViewDragMode::Started) {
		moveDrag(pos.toPoint());

	} else {
		canvas::Point point =
			mapToCanvas(timeMsec, pos, pressure, xtilt, ytilt, rotation);
		updateOutline(point);
		if(!m_prevpoint.intSame(point)) {
			if(m_pendown) {
				m_pointervelocity = point.distance(m_prevpoint);
				m_pointerdistance += m_pointervelocity;
				CanvasShortcuts::ConstraintMatch match =
					m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
				onPenMove(
					point, buttons.testFlag(Qt::RightButton),
					match.toolConstraint1(), match.toolConstraint2());

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
	const auto mousePos = compat::mousePos(*event);
	DP_EVENT_LOG(
		"mouse_move x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d "
		"pendown=%d touching=%d",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), isSynthetic(event), m_pendown,
		m_touching);

	if(isSynthetic(event) || m_pendown == TABLETDOWN || m_touching) {
		return;
	}

	if(m_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	penMoveEvent(
		QDateTime::currentMSecsSinceEpoch(), mousePos, 1.0, 0.0, 0.0, 0.0,
		event->buttons(), event->modifiers());
}

void CanvasView::penReleaseEvent(
	long long timeMsec, const QPointF &pos, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers)
{
	canvas::Point point = mapToCanvas(timeMsec, pos, 0.0, 0.0, 0.0, 0.0);
	m_prevpoint = point;
	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, Qt::LeftButton);

	if(m_dragmode != ViewDragMode::None && m_dragButton != Qt::NoButton) {
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragmode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
		default:
			m_dragmode = ViewDragMode::None;
			break;
		}

	} else if(
		m_pendown == TABLETDOWN ||
		((button == Qt::LeftButton || button == Qt::RightButton) &&
		 m_pendown == MOUSEDOWN)) {
		onPenUp();
		m_pendown = NOTDOWN;
		switch(mouseMatch.action()) {
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
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
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

void CanvasView::touchPressEvent(long long timeMsec, const QPointF &pos)
{
	penPressEvent(
		timeMsec, pos, 1.0, 0.0, 0.0, 0.0, Qt::LeftButton, Qt::NoModifier,
		false);
}

void CanvasView::touchMoveEvent(long long timeMsec, const QPointF &pos)
{
	penMoveEvent(
		timeMsec, pos, 1.0, 0.0, 0.0, 0.0, Qt::LeftButton, Qt::NoModifier);
}

void CanvasView::touchReleaseEvent(long long timeMsec, const QPointF &pos)
{
	penReleaseEvent(timeMsec, pos, Qt::LeftButton, Qt::NoModifier);
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	const auto mousePos = compat::mousePos(*event);
	DP_EVENT_LOG(
		"mouse_release x=%d y=%d buttons=0x%x modifiers=0x%x synthetic=%d "
		"pendown=%d touching=%d",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), isSynthetic(event), m_pendown,
		m_touching);
	if(isSynthetic(event) || m_touching) {
		return;
	}
	penReleaseEvent(
		QDateTime::currentMSecsSinceEpoch(), mousePos, event->button(),
		event->modifiers());
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *)
{
	// Ignore doubleclicks
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	CanvasShortcuts::Match match =
		m_canvasShortcuts.matchMouseWheel(event->modifiers(), m_keysDown);
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

	switch(match.action()) {
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
			if(match.action() == CanvasShortcuts::CANVAS_ROTATE) {
				setRotation(rotation() + steps * 10);
			} else {
				zoomStepsAt(steps, mapToCanvas(compat::wheelPosition(*event)));
			}
		}
		break;
	}
	// Color and layer picking by spinning the scroll wheel is weird, but okay.
	case CanvasShortcuts::COLOR_PICK:
		if(m_allowColorPick && m_scene->hasImage()) {
			QPointF p = mapToCanvas(compat::wheelPosition(*event));
			m_scene->model()->pickColor(p.x(), p.y(), 0, 0);
		}
		break;
	case CanvasShortcuts::LAYER_PICK: {
		if(m_scene->hasImage()) {
			QPointF p = mapToCanvas(compat::wheelPosition(*event));
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
		qWarning("Unhandled mouse wheel canvas shortcut %u", match.action());
		break;
	}
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
	QGraphicsView::keyPressEvent(event);
	if(event->isAutoRepeat()) {
		return;
	}

	m_keysDown.insert(Qt::Key(event->key()));

	if(m_dragmode == ViewDragMode::Started && m_dragButton != Qt::NoButton) {
		// There's currently some dragging with a mouse button held down going
		// on. Switch to a different flavor of drag if appropriate and bail out.
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			event->modifiers(), m_keysDown, m_dragButton);
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			resetCursor();
			updateOutline();
			break;
		default:
			break;
		}
		return;
	}

	if(m_pendown == NOTDOWN) {
		CanvasShortcuts::Match keyMatch = m_canvasShortcuts.matchKeyCombination(
			event->modifiers(), Qt::Key(event->key()));
		switch(keyMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_penmode = PenMode::Normal;
			m_dragmode = ViewDragMode::Started;
			m_dragAction = keyMatch.action();
			m_dragButton = Qt::NoButton;
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

	if(m_pendown == NOTDOWN) {
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			event->modifiers(), m_keysDown, Qt::LeftButton);
		switch(mouseMatch.action()) {
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
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
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

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
	QGraphicsView::keyReleaseEvent(event);
	if(event->isAutoRepeat()) {
		return;
	}

	bool wasDragging = m_dragmode == ViewDragMode::Started;
	if(wasDragging) {
		CanvasShortcuts::Match dragMatch =
			m_dragButton == Qt::NoButton
				? m_canvasShortcuts.matchKeyCombination(
					  event->modifiers(), Qt::Key(event->key()))
				: m_canvasShortcuts.matchMouseButton(
					  event->modifiers(), m_keysDown, m_dragButton);
		switch(dragMatch.action()) {
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

	m_keysDown.remove(Qt::Key(event->key()));

	if(wasDragging && m_dragButton != Qt::NoButton) {
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			event->modifiers(), m_keysDown, m_dragButton);
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragmode = ViewDragMode::Started;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			resetCursor();
			updateOutline();
			break;
		default:
			break;
		}
		return;
	}

	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		event->modifiers(), m_keysDown, Qt::LeftButton);
	if(m_dragmode == ViewDragMode::Prepared) {
		switch(mouseMatch.action()) {
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
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
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
		switch(mouseMatch.action()) {
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

static qreal squareDist(const QPointF &p)
{
	return p.x() * p.x() + p.y() * p.y();
}

void CanvasView::setPressureCurve(const KisCubicCurve &pressureCurve)
{
	m_pressureCurve = pressureCurve;
}

void CanvasView::touchEvent(QTouchEvent *event)
{
	event->accept();

	const auto points = compat::touchPoints(*event);
	int pointsCount = points.size();
	switch(event->type()) {
	case QEvent::TouchBegin:
		m_touchDrawBuffer.clear();
		m_touchRotating = false;
		if(m_enableTouchDraw && pointsCount == 1) {
			QPointF pos = compat::touchPos(points.first());
			DP_EVENT_LOG(
				"touch_draw_begin x=%f y=%f pendown=%d touching=%d points=%d",
				pos.x(), pos.y(), m_pendown, m_touching, pointsCount);
			if(m_enableTouchScroll || m_enableTouchPinch ||
			   m_enableTouchTwist) {
				// Buffer the touch first, since it might end up being the
				// beginning of an action that involves multiple fingers.
				m_touchDrawBuffer.append(
					{QDateTime::currentMSecsSinceEpoch(), pos});
				m_touchMode = TouchMode::Unknown;
			} else {
				// There's no other actions other than drawing enabled, so we
				// can just start drawing without awaiting what happens next.
				m_touchMode = TouchMode::Drawing;
				touchPressEvent(QDateTime::currentMSecsSinceEpoch(), pos);
			}
		} else {
			DP_EVENT_LOG(
				"touch_begin pendown=%d touching=%d points=%d", m_pendown,
				m_touching, pointsCount);
			m_touchMode = TouchMode::Moving;
		}
		break;

	case QEvent::TouchUpdate:
		if(m_enableTouchDraw &&
		   ((pointsCount == 1 && m_touchMode == TouchMode::Unknown) ||
			m_touchMode == TouchMode::Drawing)) {
			QPointF pos = compat::touchPos(compat::touchPoints(*event).first());
			DP_EVENT_LOG(
				"touch_draw_update x=%f y=%f pendown=%d touching=%d points=%d",
				pos.x(), pos.y(), m_pendown, m_touching, pointsCount);
			int bufferCount = m_touchDrawBuffer.size();
			if(bufferCount == 0) {
				if(m_touchMode == TouchMode::Drawing) {
					touchMoveEvent(QDateTime::currentMSecsSinceEpoch(), pos);
				} else { // Shouldn't happen, but we'll deal with it anyway.
					m_touchMode = TouchMode::Drawing;
					touchPressEvent(QDateTime::currentMSecsSinceEpoch(), pos);
				}
			} else {
				// This still might be the beginning of a multitouch operation.
				// If the finger didn't move enough of a distance and we didn't
				// buffer an excessive amount of touches yet. Buffer the touched
				// point and wait a bit more as to what's going to happen.
				bool shouldAppend =
					bufferCount < TOUCH_DRAW_BUFFER_COUNT &&
					QLineF{m_touchDrawBuffer.first().second, pos}.length() <
						TOUCH_DRAW_DISTANCE;
				if(shouldAppend) {
					m_touchDrawBuffer.append(
						{QDateTime::currentMSecsSinceEpoch(), pos});
				} else {
					m_touchMode = TouchMode::Drawing;
					flushTouchDrawBuffer();
					touchMoveEvent(QDateTime::currentMSecsSinceEpoch(), pos);
				}
			}
		} else {
			m_touchMode = TouchMode::Moving;

			QPointF startCenter, lastCenter, center;
			for(const auto &tp : compat::touchPoints(*event)) {
				startCenter += compat::touchStartPos(tp);
				lastCenter += compat::touchLastPos(tp);
				center += compat::touchPos(tp);
			}
			startCenter /= pointsCount;
			lastCenter /= pointsCount;
			center /= pointsCount;

			DP_EVENT_LOG(
				"touch_update x=%f y=%f pendown=%d touching=%d points=%d",
				center.x(), center.y(), m_pendown, m_touching, pointsCount);

			if(!m_touching) {
				m_touchStartZoom = zoom();
				m_touchStartRotate = rotation();
			}

			// We want to pan with one finger if one-finger pan is enabled and
			// also when pinching to zoom. Slightly non-obviously, we also want
			// to pan with one finger when finger drawing is enabled, because if
			// we got here with one finger, we've come out of a multitouch
			// operation and aren't going to be drawing until all fingers leave
			// the surface anyway, so panning is the only sensible option.
			bool havePinchOrTwist =
				pointsCount >= 2 && (m_enableTouchPinch || m_enableTouchTwist);
			if(m_enableTouchScroll || m_enableTouchDraw || havePinchOrTwist) {
				m_touching = true;
				float dx = center.x() - lastCenter.x();
				float dy = center.y() - lastCenter.y();
				horizontalScrollBar()->setValue(
					horizontalScrollBar()->value() - dx);
				verticalScrollBar()->setValue(
					verticalScrollBar()->value() - dy);
			}

			// Scaling and rotation with two fingers
			if(havePinchOrTwist) {
				m_touching = true;
				float startAvgDist = 0, avgDist = 0;
				for(const auto &tp : compat::touchPoints(*event)) {
					startAvgDist +=
						squareDist(compat::touchStartPos(tp) - startCenter);
					avgDist += squareDist(compat::touchPos(tp) - center);
				}
				startAvgDist = sqrt(startAvgDist);

				qreal zoom = m_zoom;
				if(m_enableTouchPinch) {
					avgDist = sqrt(avgDist);
					const qreal dZoom = avgDist / startAvgDist;
					zoom = m_touchStartZoom * dZoom;
				}

				qreal rotate = m_rotate;
				if(m_enableTouchTwist) {
					const auto &tps = compat::touchPoints(*event);

					const QLineF l1{
						compat::touchStartPos(tps.first()),
						compat::touchStartPos(tps.last())};
					const QLineF l2{
						compat::touchPos(tps.first()),
						compat::touchPos(tps.last())};

					const qreal dAngle = l1.angle() - l2.angle();

					// Require a small nudge to activate rotation to avoid
					// rotating when the user just wanted to zoom Alsom, only
					// rotate when touch points start out far enough from each
					// other. Initial angle measurement is inaccurate when
					// touchpoints are close together.
					if(startAvgDist / m_dpi > 0.8 &&
					   (qAbs(dAngle) > 3.0 || m_touchRotating)) {
						m_touchRotating = true;
						rotate = m_touchStartRotate + dAngle;
					}
				}

				{
					QScopedValueRollback<bool> guard{m_blockNotices, true};
					setZoom(zoom);
					setRotation(rotate);
				}

				if(m_enableTouchPinch) {
					if(m_enableTouchTwist) {
						showTransformNotice(QStringLiteral("%1\n%2").arg(
							getZoomNotice(), getRotationNotice()));
					} else {
						showTransformNotice(getZoomNotice());
					}
				} else if(m_enableTouchTwist) {
					showTransformNotice(getRotationNotice());
				}
				updateCanvasTransform([&] {
					m_zoom = zoom;
					m_rotate = rotate;
				});
			}
		}
		break;

	case QEvent::TouchEnd:
	case QEvent::TouchCancel:
		if(m_enableTouchDraw && ((m_touchMode == TouchMode::Unknown &&
								  !m_touchDrawBuffer.isEmpty()) ||
								 m_touchMode == TouchMode::Drawing)) {
			DP_EVENT_LOG(
				"touch_draw_%s pendown=%d touching=%d points=%d",
				event->type() == QEvent::TouchEnd ? "end" : "cancel", m_pendown,
				m_touching, pointsCount);
			flushTouchDrawBuffer();
			touchReleaseEvent(
				QDateTime::currentMSecsSinceEpoch(),
				compat::touchPos(compat::touchPoints(*event).first()));
		} else {
			DP_EVENT_LOG(
				"touch_%s pendown=%d touching=%d points=%d",
				event->type() == QEvent::TouchEnd ? "end" : "cancel", m_pendown,
				m_touching, pointsCount);
		}
		m_touching = false;
		break;

	default:
		break;
	}
}

void CanvasView::flushTouchDrawBuffer()
{
	int bufferCount = m_touchDrawBuffer.size();
	if(bufferCount != 0) {
		const QPair<long long, QPointF> &press = m_touchDrawBuffer.first();
		touchPressEvent(press.first, press.second);
		for(int i = 0; i < bufferCount; ++i) {
			const QPair<long long, QPointF> &move = m_touchDrawBuffer[i];
			touchMoveEvent(move.first, move.second);
		}
		m_touchDrawBuffer.clear();
	}
}

//! Handle viewport events
/**
 * Tablet events are handled here
 * @param event event info
 */
bool CanvasView::viewportEvent(QEvent *event)
{
	QEvent::Type type = event->type();
	if(type == QEvent::TouchBegin || type == QEvent::TouchUpdate ||
	   type == QEvent::TouchEnd || type == QEvent::TouchCancel) {
		touchEvent(static_cast<QTouchEvent *>(event));
	} else if(type == QEvent::TabletPress && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		DP_EVENT_LOG(
			"tablet_press spontaneous=%d x=%f y=%f pressure=%f xtilt=%d "
			"ytilt=%d rotation=%f buttons=0x%x modifiers=0x%x pendown=%d "
			"touching=%d",
			tabev->spontaneous(), tabPos.x(), tabPos.y(), tabev->pressure(),
			compat::cast_6<int>(tabev->xTilt()),
			compat::cast_6<int>(tabev->yTilt()),
			qDegreesToRadians(tabev->rotation()), unsigned(tabev->buttons()),
			unsigned(tabev->modifiers()), m_pendown, m_touching);

		// Note: it is possible to get a mouse press event for a tablet event
		// (even before the tablet event is received or even though
		// tabev->accept() is called), but it is never possible to get a
		// TabletPress for a real mouse press. Therefore, we don't actually do
		// anything yet in the penDown handler other than remember the initial
		// point and we'll let a TabletEvent override the mouse event. When
		// KIS_TABLET isn't enabled we ignore synthetic mouse events though.
		if(!tabletinput::passPenEvents()) {
			tabev->accept();
		}

		penPressEvent(
			QDateTime::currentMSecsSinceEpoch(), compat::tabPosF(*tabev),
			tabev->pressure(), tabev->xTilt(), tabev->yTilt(),
			qDegreesToRadians(tabev->rotation()), tabev->button(),
			QApplication::queryKeyboardModifiers(), // TODO check if tablet
													// event modifiers() is
													// still broken in Qt 5.12
			true);
	} else if(type == QEvent::TabletMove && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		DP_EVENT_LOG(
			"tablet_move spontaneous=%d x=%f y=%f pressure=%f xtilt=%d "
			"ytilt=%d rotation=%f buttons=0x%x modifiers=0x%x pendown=%d "
			"touching=%d",
			tabev->spontaneous(), tabPos.x(), tabPos.y(), tabev->pressure(),
			compat::cast_6<int>(tabev->xTilt()),
			compat::cast_6<int>(tabev->yTilt()),
			qDegreesToRadians(tabev->rotation()), unsigned(tabev->buttons()),
			unsigned(tabev->modifiers()), m_pendown, m_touching);

		if(!tabletinput::passPenEvents()) {
			tabev->accept();
		}

		penMoveEvent(
			QDateTime::currentMSecsSinceEpoch(), compat::tabPosF(*tabev),
			tabev->pressure(), tabev->xTilt(), tabev->yTilt(),
			qDegreesToRadians(tabev->rotation()), tabev->buttons(),
			QApplication::queryKeyboardModifiers() // TODO check if tablet event
												   // modifiers() is still
												   // broken in Qt 5.12
		);
	} else if(type == QEvent::TabletRelease && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		DP_EVENT_LOG(
			"tablet_release spontaneous=%d x=%f y=%f buttons=0x%x pendown=%d "
			"touching=%d",
			tabev->spontaneous(), tabPos.x(), tabPos.y(),
			unsigned(tabev->buttons()), m_pendown, m_touching);
		if(!tabletinput::passPenEvents()) {
			tabev->accept();
		}
		// TODO check if tablet event modifiers() is still broken in Qt 5.12
		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), tabPos, tabev->button(),
			QApplication::queryKeyboardModifiers());
	} else {
		return QGraphicsView::viewportEvent(event);
	}

	return true;
}

void CanvasView::updateOutline(QPointF point)
{
	if(m_showoutline && !m_lock && !m_busy &&
	   (!m_prevoutline ||
		!canvas::Point::roughlySame(point, m_prevoutlinepoint))) {
		if(!m_subpixeloutline) {
			point.setX(qFloor(point.x()) + 0.5);
			point.setY(qFloor(point.y()) + 0.5);
		}
		updateScene(
			{getOutlineBounds(m_prevoutlinepoint, m_outlineSize),
			 getOutlineBounds(point, m_outlineSize)});
		m_prevoutlinepoint = point;
	}
}

void CanvasView::updateOutline()
{
	updateScene({getOutlineBounds(m_prevoutlinepoint, m_outlineSize)});
}

QRectF CanvasView::getOutlineBounds(const QPointF &point, int size)
{
	qreal owidth = (size + m_brushOutlineWidth) * m_zoom;
	qreal orad = owidth / 2.0;
	QPointF mapped = mapFromCanvas(point);
	return QRectF{mapped.x() - orad, mapped.y() - orad, owidth, owidth};
}

QPoint CanvasView::viewCenterPoint() const
{
	return mapToCanvas(rect().center()).toPoint();
}

bool CanvasView::isPointVisible(const QPointF &point) const
{
	return mapToCanvas(viewport()->rect())
		.containsPoint(point, Qt::OddEvenFill);
}

void CanvasView::scrollTo(const QPointF &point)
{
	updateCanvasTransform([&] {
		QTransform matrix = calculateCanvasTransformFrom(
			QPointF{}, m_zoom, m_rotate, m_mirror, m_flip);
		m_pos = matrix.map(point);
	});
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

void CanvasView::updateCanvasTransform(const std::function<void()> &block)
{
	QList<QRectF> rects;
	QPointF outlinePoint;
	if(m_prevoutline) {
		outlinePoint = fromCanvasTransform().map(m_prevoutlinepoint);
		rects.append(getOutlineBounds(m_prevoutlinepoint, m_outlineSize));
	}

	block();

	if(m_scene) {
		updatePosBounds();
		rects.append(m_scene->canvasBounds());
		m_scene->setCanvasTransform(calculateCanvasTransform());
		rects.append(m_scene->canvasBounds());
		updateScrollBars();
		updateCanvasPixelGrid();
		setRenderHint(QPainter::SmoothPixmapTransform, m_zoom <= 1.99);
		viewRectChanged();
	}

	if(m_prevoutline) {
		m_prevoutlinepoint = toCanvasTransform().map(outlinePoint);
		rects.append(getOutlineBounds(m_prevoutlinepoint, m_outlineSize));
	}
	updateScene(rects);
}

void CanvasView::updatePosBounds()
{
	if(m_scene && m_scene->hasImage()) {
		QTransform matrix = calculateCanvasTransformFrom(
			QPointF{}, m_zoom, m_rotate, m_mirror, m_flip);
		QRectF cr{QPointF{}, QSizeF{m_scene->model()->size()}};
		QRectF vr{viewport()->rect()};
		m_posBounds = matrix.map(cr)
						  .boundingRect()
						  .translated(-mapToScene(QPoint{0, 0}))
						  .adjusted(-vr.width(), -vr.height(), 0.0, 0.0)
						  .marginsRemoved(QMarginsF{64.0, 64.0, 64.0, 64.0});
		clampPosition();
	}
}

void CanvasView::clampPosition()
{
	if(m_posBounds.isValid()) {
		m_pos.setX(qBound(m_posBounds.left(), m_pos.x(), m_posBounds.right()));
		m_pos.setY(qBound(m_posBounds.top(), m_pos.y(), m_posBounds.bottom()));
	}
}

void CanvasView::updateScrollBars()
{
	QScopedValueRollback<bool> guard{m_scrollBarsAdjusting, true};
	QScrollBar *hbar = horizontalScrollBar();
	QScrollBar *vbar = verticalScrollBar();
	if(m_posBounds.isValid()) {
		QRect page =
			toCanvasTransform().mapToPolygon(viewport()->rect()).boundingRect();

		hbar->setRange(m_posBounds.left(), m_posBounds.right());
		hbar->setValue(m_pos.x());
		hbar->setPageStep(page.width() * m_zoom);
		hbar->setSingleStep(qMax(1, hbar->pageStep() / 20));

		vbar->setRange(m_posBounds.top(), m_posBounds.bottom());
		vbar->setValue(m_pos.y());
		vbar->setPageStep(page.height() * m_zoom);
		vbar->setSingleStep(qMax(1, vbar->pageStep() / 20));
	} else {
		hbar->setRange(0, 0);
		vbar->setRange(0, 0);
	}
}

void CanvasView::updateCanvasPixelGrid()
{
	if(m_scene) {
		m_scene->setCanvasPixelGrid(m_pixelgrid && m_zoom >= 8.0);
	}
}

QTransform CanvasView::calculateCanvasTransform() const
{
	return calculateCanvasTransformFrom(
		m_pos, m_zoom, m_rotate, m_mirror, m_flip);
}

QTransform CanvasView::calculateCanvasTransformFrom(
	const QPointF &pos, qreal zoom, qreal rotate, bool mirror, bool flip)
{
	QTransform matrix;
	matrix.translate(-pos.x(), -pos.y());
	matrix.scale(zoom, zoom);
	mirrorFlip(matrix, mirror, flip);
	matrix.rotate(rotate);
	return matrix;
}

void CanvasView::mirrorFlip(QTransform &matrix, bool mirror, bool flip)
{
	matrix.scale(mirror ? -1.0 : 1.0, flip ? -1.0 : 1.0);
}

void CanvasView::emitViewTransformed()
{
	emit viewTransformed(zoom(), rotation());
}

bool CanvasView::isRotationInverted() const
{
	return m_mirror ^ m_flip;
}

/**
 * @brief accept image drops
 * @param event event info
 *
 * @todo Check file extensions
 */
void CanvasView::dragEnterEvent(QDragEnterEvent *event)
{
	if(event->mimeData()->hasUrls() || event->mimeData()->hasImage() ||
	   event->mimeData()->hasColor())
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
		emit imageDropped(
			qvariant_cast<QImage>(event->mimeData()->imageData()));
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
	QScopedValueRollback<bool> guard{m_scrollBarsAdjusting, true};
	QGraphicsView::showEvent(event);
	// Find the DPI of the screen
	// TODO: if the window is moved to another screen, this should be
	// updated
	QWidget *w = this;
	while(w) {
		if(w->windowHandle() != nullptr) {
			m_dpi = w->windowHandle()->screen()->physicalDotsPerInch();
			break;
		}
		w = w->parentWidget();
	}
}

void CanvasView::resizeEvent(QResizeEvent *e)
{
	QScopedValueRollback<bool> guard{m_scrollBarsAdjusting, true};
	QGraphicsView::resizeEvent(e);
	if(!e->size().isEmpty()) {
		updateOutline(mapToCanvas(mapFromGlobal(QCursor::pos())));
		updateCanvasTransform([] {
			// Nothing.
		});
	}
}

void CanvasView::scrollContentsBy(int dx, int dy)
{
	if(!m_scrollBarsAdjusting) {
		scrollBy(-dx, -dy);
	}
}

QString CanvasView::getZoomNotice() const
{
	return tr("Zoom: %1%").arg(zoom() * 100.0, 0, 'f', 2);
}

QString CanvasView::getRotationNotice() const
{
	return tr("Rotation: %1°").arg(rotation(), 0, 'f', 2);
}

void CanvasView::showTransformNotice(const QString &text)
{
	if(m_scene && !m_blockNotices) {
		m_scene->showTransformNotice(text);
	}
}

void CanvasView::updateLockNotice()
{
	if(m_scene) {
		if(m_lock) {
			m_scene->showLockNotice(lockDescription());
		} else {
			m_scene->hideLockNotice();
		}
	}
}

}
