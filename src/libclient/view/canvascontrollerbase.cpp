// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/view/canvascontrollerbase.h"
#include "libclient/canvas/blendmodes.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/utils/cursors.h"
#include "libclient/utils/qtguicompat.h"
#include "libclient/view/enums.h"
#include "libclient/view/touchhandler.h"
#include "libclient/view/zoom.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <cmath>
#ifdef DP_CANVAS_CONTROLLER_HOVER_EVENTS
#	include <QHoverEvent>
#endif

using utils::Cursors;

namespace view {

class CanvasControllerBase::SetDragParams {
public:
	static SetDragParams fromNone()
	{
		return SetDragParams(
			CanvasShortcuts::Action::NO_ACTION, Qt::NoButton, Qt::NoModifier,
			false, false, int(ViewDragMode::None));
	}

	static SetDragParams fromKeyMatch(const CanvasShortcuts::Match &match)
	{
		return SetDragParams(
			match.action(), Qt::NoButton, match.shortcut->mods,
			match.inverted(), match.swapAxes(), -1);
	}

	static SetDragParams fromMouseMatch(const CanvasShortcuts::Match &match)
	{
		return SetDragParams(
			match.action(), match.shortcut->button, match.shortcut->mods,
			match.inverted(), match.swapAxes(), -1);
	}

	SetDragParams(
		CanvasShortcuts::Action action, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers, bool inverted, bool swapAxes,
		int dragMode)
		: m_action(action)
		, m_button(button)
		, m_modifiers(modifiers)
		, m_dragMode(dragMode)
		, m_inverted(inverted)
		, m_swapAxes(swapAxes)
	{
	}

	CanvasShortcuts::Action action() const { return m_action; }
	Qt::MouseButton button() const { return m_button; }
	Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
	bool inverted() const { return m_inverted; }
	bool swapAxes() const { return m_swapAxes; }

	bool hasPenMode() const { return m_penMode != -1; }
	PenMode penMode() const { return PenMode(m_penMode); }
	SetDragParams &setPenMode(PenMode penMode)
	{
		m_penMode = int(penMode);
		return *this;
	}

	bool isDragConditionFulfilled() const { return m_dragCondition; }
	SetDragParams &setDragCondition(bool dragCondition)
	{
		m_dragCondition = dragCondition;
		return *this;
	}
	SetDragParams &setDragConditionClearDragModeOnFailure(bool dragCondition)
	{
		m_dragCondition = dragCondition;
		m_clearDragModeOnFailedCondition = true;
		return *this;
	}

	bool hasDragMode() const { return m_dragMode != -1; }
	ViewDragMode dragMode() const { return ViewDragMode(m_dragMode); }
	SetDragParams &setDragMode(ViewDragMode dragMode)
	{
		m_dragMode = int(dragMode);
		return *this;
	}

	bool resetDragPoints() const { return m_resetDragPoints; }
	SetDragParams &setResetDragPoints()
	{
		m_resetDragPoints = true;
		return *this;
	}

	bool resetDragRotation() const { return m_resetDragRotation; }
	SetDragParams &setResetDragRotation()
	{
		m_resetDragRotation = true;
		return *this;
	}

	bool updateOutline() const { return m_updateOutline; }
	SetDragParams &setUpdateOutline()
	{
		m_updateOutline = true;
		return *this;
	}

	bool resetCursor() const { return m_resetCursor; }
	SetDragParams &setResetCursor()
	{
		m_resetCursor = true;
		return *this;
	}

private:
	CanvasShortcuts::Action m_action;
	Qt::MouseButton m_button;
	Qt::KeyboardModifiers m_modifiers;
	int m_penMode = -1;
	int m_dragMode;
	bool m_inverted;
	bool m_swapAxes;
	bool m_dragCondition = true;
	bool m_clearDragModeOnFailedCondition = false;
	bool m_resetDragPoints = false;
	bool m_resetDragRotation = false;
	bool m_updateOutline = false;
	bool m_resetCursor = false;
};

CanvasControllerBase::CanvasControllerBase(QObject *parent)
	: QObject(parent)
	, m_brushCursorStyle(int(Cursor::TriangleRight))
	, m_eraseCursorStyle(int(Cursor::SameAsBrush))
	, m_alphaLockCursorStyle(int(Cursor::SameAsBrush))
	, m_brushBlendMode(DP_BLEND_MODE_NORMAL)
	, m_toolState(int(tools::ToolState::Normal))
#ifdef Q_OS_LINUX
	, m_waylandWorkarounds(
		  QGuiApplication::platformName() == QStringLiteral("wayland"))
#endif
{
}

void CanvasControllerBase::setCanvasModel(canvas::CanvasModel *canvasModel)
{
	m_canvasModel = canvasModel;
	updateCanvasSize(0, 0, 0, 0);
	if(canvasModel) {
		canvas::PaintEngine *pe = canvasModel->paintEngine();
		connect(
			pe, &canvas::PaintEngine::tileCacheDirtyCheckNeeded, this,
			&CanvasControllerBase::tileCacheDirtyCheckNeeded,
			pe->isTileCacheDirtyCheckOnTick() ? Qt::DirectConnection
											  : Qt::QueuedConnection);
	}
}

bool CanvasControllerBase::shouldRenderSmooth() const
{
	return m_renderSmooth && m_zoom <= 1.99 &&
		   !(std::fabs(m_zoom - 1.0) < 0.01 &&
			 std::fmod(std::fabs(rotation()), 90.0) < 0.01);
}

bool CanvasControllerBase::isTouchDrawEnabled() const
{
	return touchHandler()->isTouchDrawEnabled();
}

bool CanvasControllerBase::isTouchPanEnabled() const
{
	return touchHandler()->isTouchPanEnabled();
}

bool CanvasControllerBase::isTouchDrawOrPanEnabled() const
{
	return touchHandler()->isTouchDrawOrPanEnabled();
}

void CanvasControllerBase::setCanvasVisible(bool canvasVisible)
{
	if(m_canvasVisible != canvasVisible) {
		m_canvasVisible = canvasVisible;
		emit canvasVisibleChanged();
	}
}

qreal CanvasControllerBase::rotation() const
{
	qreal r = isRotationInverted() ? 360.0 - m_rotation : m_rotation;
	return r > 180.0 ? r - 360.0 : r;
}

void CanvasControllerBase::scrollTo(const QPointF &point)
{
	updateCanvasTransform([&] {
		QTransform matrix = calculateCanvasTransformFrom(
			QPointF(), m_zoom, m_rotation, m_mirror, m_flip);
		m_pos = matrix.map(
			point - viewTransformOffset() / (m_zoom > 0.0 ? m_zoom : 1.0));
	});
}

void CanvasControllerBase::scrollBy(int x, int y)
{
	scrollByF(x, y);
}

void CanvasControllerBase::scrollByF(qreal x, qreal y)
{
	updateCanvasTransform([&] {
		m_pos.setX(m_pos.x() + x);
		m_pos.setY(m_pos.y() + y);
	});
}

void CanvasControllerBase::setZoom(qreal zoom)
{
	setZoomAt(zoom, mapPointToCanvasF(viewCenterF()));
}

void CanvasControllerBase::setZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(getZoomMin(), zoom, getZoomMax());
	if(newZoom != m_zoom) {
		QTransform matrix;
		mirrorFlip(matrix, m_mirror, m_flip);
		matrix.rotate(m_rotation);

		updateCanvasTransform([&] {
			m_pos +=
				matrix.map(point * (actualZoomFor(newZoom) - actualZoom()));
			m_zoom = newZoom;
		});

		showTransformNotice(getZoomNoticeText());
	}
}

void CanvasControllerBase::resetZoomCenter()
{
	setZoom(1.0);
}

void CanvasControllerBase::resetZoomCursor()
{
	setZoomAt(1.0, cursorPosOrCenter());
}

void CanvasControllerBase::zoomTo(const QRect &rect, int steps)
{
	if(steps < 0 || rect.width() < ZOOM_TO_MINIMUM_DIMENSION ||
	   rect.height() < ZOOM_TO_MINIMUM_DIMENSION) {
		zoomStepsAt(steps, rect.center());
	} else {
		QRectF r = mapRectFromCanvas(rect).boundingRect();
		QSizeF vs = viewSizeF();
		qreal xScale = vs.width() / r.width();
		qreal yScale = vs.height() / r.height();
		setZoomAt(m_zoom * qMin(xScale, yScale), rect.center());
	}
}

void CanvasControllerBase::zoomToFit()
{
	setZoomToFit(Qt::Horizontal | Qt::Vertical);
}

void CanvasControllerBase::zoomInCenter()
{
	zoomSteps(1);
}

void CanvasControllerBase::zoomInCursor()
{
	zoomStepsAt(1, cursorPosOrCenter());
}

void CanvasControllerBase::zoomOutCenter()
{
	zoomSteps(-1);
}

void CanvasControllerBase::zoomOutCursor()
{
	zoomStepsAt(-1, cursorPosOrCenter());
}

void CanvasControllerBase::zoomToFitWidth()
{
	setZoomToFit(Qt::Horizontal);
}

void CanvasControllerBase::zoomToFitHeight()
{
	setZoomToFit(Qt::Vertical);
}

void CanvasControllerBase::zoomSteps(int steps)
{
	zoomStepsAt(steps, mapPointToCanvasF(viewCenterF()));
}

void CanvasControllerBase::zoomStepsAt(int steps, const QPointF &point)
{
	constexpr qreal eps = 1e-5;
	const QVector<qreal> &zoomLevels = getZoomLevels();
	// This doesn't actually take the number of steps into account, it just
	// zooms by a single step. But that works really well, so I'll leave it be.
	if(steps > 0) {
		int i = 0;
		while(i < zoomLevels.size() - 1 && m_zoom > zoomLevels[i] - eps) {
			i++;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom > level - eps ? getZoomMax() : qMax(m_zoom, level);
		setZoomAt(zoom, point);
	} else if(steps < 0) {
		int i = zoomLevels.size() - 1;
		while(i > 0 && m_zoom < zoomLevels[i] + eps) {
			i--;
		}
		qreal level = zoomLevels[i];
		qreal zoom = m_zoom < level + eps ? getZoomMin() : qMin(m_zoom, level);
		setZoomAt(zoom, point);
	}
}

void CanvasControllerBase::setRotation(qreal degrees)
{
	degrees = std::fmod(degrees, 360.0);
	if(degrees < 0.0) {
		degrees += 360.0;
	}

	bool inverted = isRotationInverted();
	if(inverted) {
		degrees = 360.0 - degrees;
	}

	if(degrees != m_rotation) {
		QTransform prev, cur;
		prev.rotate(m_rotation);
		cur.rotate(degrees);
		translateByViewTransformOffset(prev, cur);

		updateCanvasTransform([&] {
			if(inverted) {
				m_pos = cur.inverted().map(prev.map(m_pos));
			} else {
				m_pos = prev.inverted().map(cur.map(m_pos));
			}
			m_rotation = degrees;
		});

		showTransformNotice(getRotationNoticeText());
	}
}

void CanvasControllerBase::resetRotation()
{
	setRotation(0.0);
}

void CanvasControllerBase::rotateStepClockwise()
{
	setRotation(rotation() + ROTATION_STEP);
}

void CanvasControllerBase::rotateStepCounterClockwise()
{
	setRotation(rotation() - ROTATION_STEP);
}

void CanvasControllerBase::setFlip(bool flip)
{
	if(flip != m_flip) {
		QTransform prev, cur;
		mirrorFlip(prev, m_mirror, m_flip);
		mirrorFlip(cur, m_mirror, flip);
		translateByViewTransformOffset(prev, cur);

		updateCanvasTransform([&] {
			m_pos = cur.inverted().map(prev.map(m_pos));
			m_flip = flip;
		});

		showTransformNotice(
			m_flip ? QCoreApplication::translate(
						 "widgets::CanvasView", "Vertical flip: ON")
				   : QCoreApplication::translate(
						 "widgets::CanvasView", "Vertical flip: OFF"));
	}
}

void CanvasControllerBase::setMirror(bool mirror)
{
	if(mirror != m_mirror) {
		QTransform prev, cur;
		mirrorFlip(prev, m_mirror, m_flip);
		mirrorFlip(cur, mirror, m_flip);
		translateByViewTransformOffset(prev, cur);

		updateCanvasTransform([&] {
			m_pos = cur.inverted().map(prev.map(m_pos));
			m_mirror = mirror;
		});

		showTransformNotice(
			m_mirror ? QCoreApplication::translate(
						   "widgets::CanvasView", "Horizontal mirror: ON")
					 : QCoreApplication::translate(
						   "widgets::CanvasView", "Horizontal mirror: OFF"));
	}
}

QPoint CanvasControllerBase::viewCenterPoint() const
{
	return mapPointToCanvasF(viewCenterF()).toPoint();
}

bool CanvasControllerBase::isPointVisible(const QPointF &point) const
{
	return mapRectToCanvasF(viewRectF()).containsPoint(point, Qt::OddEvenFill);
}

QRectF CanvasControllerBase::screenRect() const
{
	return mapRectToCanvasF(viewRectF()).boundingRect();
}

void CanvasControllerBase::updateViewSize()
{
	resetCanvasTransform();
	handleViewSizeChange();
	emitScrollAreaChanged();
	emitViewRectChanged();
}

void CanvasControllerBase::updateCanvasSize(
	int width, int height, int offsetX, int offsetY)
{
	QSize oldSize = m_canvasSize;
	m_canvasSize = QSize(width, height);

	{
		QScopedValueRollback<bool> canvasSizeChangingRollback(
			m_canvasSizeChanging, true);
		if(oldSize.isEmpty()) {
			QScopedValueRollback<bool> blockNoticesRollback(
				m_blockNotices, true);
			zoomToFit();
		} else {
			scrollByF(offsetX * m_zoom, offsetY * m_zoom);
		}
	}

	emitScrollAreaChanged();
	emitViewRectChanged();

	if(offsetX != 0 || offsetY != 0) {
		emit canvasOffset(offsetX, offsetY);
	}
}

void CanvasControllerBase::withTileCache(
	const std::function<void(canvas::TileCache &)> &fn)
{
	if(m_canvasModel) {
		m_canvasModel->paintEngine()->withTileCache(fn);
	}
}

void CanvasControllerBase::handleEnter()
{
	m_showOutline = true;
	setCursorOnCanvas(true);
	updateOutline();
	resetTabletFilter();
}

void CanvasControllerBase::handleLeave()
{
	m_showOutline = false;
	setCursorOnCanvas(false);
	m_hudActionToActivate.clear();
	updateOutline();
	clearHudHover();
	resetTabletFilter();
}

void CanvasControllerBase::handleFocusIn()
{
	clearKeys();
}

void CanvasControllerBase::clearKeys()
{
	m_keysDown.clear();
	setDrag(
		SetDragParams::fromNone()
			.setPenMode(PenMode::Normal)
			.setUpdateOutline()
			.setResetCursor());
}

#ifdef DP_CANVAS_CONTROLLER_HOVER_EVENTS
void CanvasControllerBase::handleHoverMove(QHoverEvent *event)
{
	QPointF posf = event->position();
	bool touching = touchHandler()->isTouching();
	Qt::KeyboardModifiers modifiers = event->modifiers();
	QInputDevice::DeviceType deviceType = event->deviceType();
	QPointingDevice::PointerType pointerType = event->pointerType();
	bool tablet = deviceType == QInputDevice::DeviceType::Stylus ||
				  deviceType == QInputDevice::DeviceType::Airbrush ||
				  pointerType == QPointingDevice::PointerType::Pen ||
				  pointerType == QPointingDevice::PointerType::Eraser;
	DP_EVENT_LOG(
		"hover_move spontaneous=%d x=%f y=%f modifiers=0x%x deviceType=%d "
		"pointerType=%d tablet=%d penstate=%d touching=%d timestamp=%llu",
		int(event->spontaneous()), posf.x(), posf.y(), unsigned(modifiers),
		int(deviceType), int(pointerType), int(tablet), int(m_penState),
		int(touching), qulonglong(event->timestamp()));

	bool shouldHandle;
	if(m_penState == PenState::Up) {
		if(tablet) {
			shouldHandle = m_tabletEnabled;
		} else {
			shouldHandle =
				!touching && (!m_tabletEnabled || !isSyntheticHover(event));
		}
	} else {
		shouldHandle = false;
	}

	if(shouldHandle) {
		if(tablet) {
			startTabletEventTimer();
		}
		event->accept();
		penMoveEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, 0.0, 0.0, 0.0, 0.0,
			modifiers);
	}
}
#endif

void CanvasControllerBase::handleMouseMove(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	Qt::MouseButtons buttons = event->buttons();
	bool touching = touchHandler()->isTouching();
	DP_EVENT_LOG(
		"mouse_move x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(buttons), unsigned(event->modifiers()),
		unsigned(event->source()), int(m_penState), int(touching),
		qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   m_penState != PenState::TabletDown && !touching &&
	   (m_penState == PenState::Up || m_tabletEventTimer.hasExpired())) {
		if(m_penState != PenState::Up && buttons == Qt::NoButton) {
			handleMouseRelease(event);
		} else {
			event->accept();
			penMoveEvent(
				QDateTime::currentMSecsSinceEpoch(), posf, 1.0, 0.0, 0.0, 0.0,
				getMouseModifiers(event));
		}
	}
}

void CanvasControllerBase::handleMousePress(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	bool touching = touchHandler()->isTouching();
	DP_EVENT_LOG(
		"mouse_press x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(touching), qulonglong(event->timestamp()));

	Qt::MouseButton button = event->button();
	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   !touching &&
	   (button != Qt::LeftButton || m_tabletEventTimer.hasExpired())) {
		event->accept();
		penPressEvent(
			QDateTime::currentMSecsSinceEpoch(), posf,
			compat::globalPos(*event), 1.0, 0.0, 0.0, 0.0, button,
			getMouseModifiers(event), int(tools::DeviceType::Mouse), false);
	}

#ifdef Q_OS_WINDOWS
	// On Windows, we get a garbage mouse press after every tablet input, so
	// this is the point where we open the menu.
	if(m_hudActionToActivate.isValid() &&
	   m_hudActionToActivate.type == HudAction::Type::TriggerMenu &&
	   m_hudActionDeviceType == int(tools::DeviceType::Tablet)) {
		activatePendingHudAction();
	}
#endif
}

void CanvasControllerBase::handleMouseRelease(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	bool touching = touchHandler()->isTouching();
	DP_EVENT_LOG(
		"mouse_release x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(touching), qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   !touching) {
		event->accept();
		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, event->button(),
			getMouseModifiers(event));
	}
}

void CanvasControllerBase::handleTabletMove(QTabletEvent *event)
{
	if(m_tabletEnabled) {
		event->accept();

		QPointF posf = tabletPosF(event);
		qreal rotation = qDegreesToRadians(event->rotation());
		qreal pressure = event->pressure();
		qreal xTilt = event->xTilt();
		qreal yTilt = event->yTilt();
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		Qt::MouseButtons buttons = event->buttons();
		bool ignore = shouldFilterTabletEvent(event);
		DP_EVENT_LOG(
			"tablet_move spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d pointertype=%d",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(buttons), unsigned(event->modifiers()),
			int(m_penState), int(touchHandler()->isTouching()),
			unsigned(modifiers), int(ignore), compat::pointerType(*event));
		if(ignore) {
			return;
		}

		// Under Windows Ink, some tablets report bogus zero-pressure inputs.
		// We accept them so that they don't result in synthesized mouse events,
		// but don't actually act on them. Getting zero-pressure inputs for
		// buttons other than the left button (i.e. the actual pen) is expected
		// though, so we do handle those.
		if(m_penState == PenState::Up || pressure != 0.0 ||
		   buttons != Qt::LeftButton) {
			startTabletEventTimer();
			penMoveEvent(
				QDateTime::currentMSecsSinceEpoch(), posf,
				qBound(0.0, pressure, 1.0), xTilt, yTilt, rotation, modifiers);
		}
	}
}

void CanvasControllerBase::handleTabletPress(QTabletEvent *event)
{
	if(m_tabletEnabled) {
		event->accept();
		startTabletEventTimer();

		QPointF posf = tabletPosF(event);
		qreal rotation = qDegreesToRadians(event->rotation());
		Qt::MouseButtons buttons = event->buttons();
		qreal pressure = event->pressure();
		qreal xTilt = event->xTilt();
		qreal yTilt = event->yTilt();
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		bool ignore = shouldFilterTabletEvent(event);
		DP_EVENT_LOG(
			"tablet_press spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d pointertype=%d",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(buttons), unsigned(event->modifiers()),
			int(m_penState), int(touchHandler()->isTouching()),
			unsigned(modifiers), int(ignore), compat::pointerType(*event));
		if(ignore) {
			return;
		}

		Qt::MouseButton button = event->button();
		bool eraserOverride = false;
#if defined(__EMSCRIPTEN__)
		// In the browser, we don't get eraser proximity events, instead an
		// eraser pressed down is reported as button flag 0x20 (Qt::TaskButton).
		if(buttons.testFlag(Qt::TaskButton)) {
			button = Qt::LeftButton;
			eraserOverride = m_enableEraserOverride;
		}
#elif defined(Q_OS_ANDROID)
		// On Android, there's also no proximity events, but we can check the
		// pointing device whether it's an eraser or not.
		if(compat::isEraser(*event)) {
			button = Qt::LeftButton;
			eraserOverride = m_enableEraserOverride;
		}
#endif

		penPressEvent(
			QDateTime::currentMSecsSinceEpoch(), posf,
			compat::tabGlobalPos(*event), qBound(0.0, pressure, 1.0), xTilt,
			yTilt, rotation, button, modifiers, int(tools::DeviceType::Tablet),
			eraserOverride);
	}
}

void CanvasControllerBase::handleTabletRelease(QTabletEvent *event)
{
	if(m_tabletEnabled) {
		event->accept();
		startTabletEventTimer();

		QPointF posf = tabletPosF(event);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		bool ignore = shouldFilterTabletEvent(event);
		DP_EVENT_LOG(
			"tablet_release spontaneous=%d x=%f y=%f buttons=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d pointertype=%d",
			int(event->spontaneous()), posf.x(), posf.y(),
			unsigned(event->buttons()), int(m_penState),
			int(touchHandler()->isTouching()), unsigned(modifiers), int(ignore),
			compat::pointerType(*event));
		if(ignore) {
			return;
		}

		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, event->button(),
			modifiers);
	}
}

void CanvasControllerBase::handleTouchBegin(QTouchEvent *event)
{
	event->accept();
	if(!m_hudActionToActivate.isValid()) {
		const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
		int pointsCount = points.size();
		if(pointsCount > 0) {
			const compat::TouchPoint &firstPoint = points.constFirst();
			HudAction action = checkHudHover(compat::touchPos(firstPoint));
			if(action.isValid()) {
				m_hudActionToActivate = action;
				m_hudActionGlobalPos = compat::touchGlobalPos(firstPoint);
				m_hudActionDeviceType = int(tools::DeviceType::Touch);
			} else {
				touchHandler()->handleTouchBegin(event);
			}
		}
	}
}

void CanvasControllerBase::handleTouchUpdate(QTouchEvent *event)
{
	event->accept();
	if(!m_hudActionToActivate.isValid()) {
		touchHandler()->handleTouchUpdate(
			event, zoom(), rotation(), devicePixelRatioF());
	}
}

void CanvasControllerBase::handleTouchEnd(QTouchEvent *event, bool cancel)
{
	event->accept();
	if(!activatePendingHudAction()) {
		touchHandler()->handleTouchEnd(event, cancel);
	}
}

void CanvasControllerBase::handleWheel(QWheelEvent *event)
{
	event->accept();
	QPoint angleDelta = event->angleDelta();
	QPointF posf = wheelPosF(event);
	DP_EVENT_LOG(
		"wheel dx=%d dy=%d posX=%f posY=%f buttons=0x%x modifiers=0x%x "
		"penstate=%d touching=%d",
		angleDelta.x(), angleDelta.y(), posf.x(), posf.y(),
		unsigned(event->buttons()), unsigned(event->modifiers()),
		int(m_penState), int(touchHandler()->isTouching()));

	Qt::KeyboardModifiers modifiers = getWheelModifiers(event);
	CanvasShortcuts::Match match =
		m_canvasShortcuts.matchMouseWheel(modifiers, m_keysDown);
	int deltaX = angleDelta.x();
	int deltaY = angleDelta.y();
	if(match.inverted()) {
		deltaX = -deltaX;
		deltaY = -deltaY;
	}
	if(match.swapAxes()) {
		std::swap(deltaX, deltaY);
	}

	CanvasShortcuts::Action action = match.action();
	switch(action) {
	case CanvasShortcuts::NO_ACTION:
		break;
	case CanvasShortcuts::CANVAS_PAN:
		event->accept();
		scrollBy(-deltaX, -deltaY);
		break;
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
	case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
	case CanvasShortcuts::CANVAS_ZOOM: {
		event->accept();
		m_zoomWheelDelta += deltaY;
		int steps = m_zoomWheelDelta / 120;
		m_zoomWheelDelta -= steps * 120;
		if(steps != 0) {
			if(action == CanvasShortcuts::CANVAS_ROTATE) {
				setRotationSnap(rotation() + steps * 10);
			} else if(action == CanvasShortcuts::CANVAS_ROTATE_DISCRETE) {
				rotateByDiscreteSteps(steps);
			} else if(action == CanvasShortcuts::CANVAS_ROTATE_NO_SNAP) {
				setRotation(rotation() + steps * 10);
			} else {
				zoomStepsAt(steps, mapPointToCanvasF(posf));
			}
		}
		break;
	}
	// Color and layer picking by spinning the scroll wheel is weird, but okay.
	case CanvasShortcuts::COLOR_PICK:
		event->accept();
		if(toolAllowsColorPick() && m_canvasModel) {
			QPointF p = mapPointToCanvasF(posf);
			m_canvasModel->pickColor(p.x(), p.y(), 0, 0);
			m_canvasModel->finishColorPick();
		}
		break;
	case CanvasShortcuts::LAYER_PICK: {
		event->accept();
		if(m_canvasModel) {
			QPointF p = mapPointToCanvasF(posf);
			m_canvasModel->pickLayer(p.x(), p.y());
		}
		break;
	}
	case CanvasShortcuts::TOOL_ADJUST1:
		wheelAdjust(
			event, int(tools::QuickAdjustType::Tool1), toolAllowsToolAdjust1(),
			deltaY);
		break;
	case CanvasShortcuts::TOOL_ADJUST2:
		wheelAdjust(
			event, int(tools::QuickAdjustType::Tool2), toolAllowsToolAdjust2(),
			deltaY);
		break;
	case CanvasShortcuts::TOOL_ADJUST3:
		wheelAdjust(
			event, int(tools::QuickAdjustType::Tool3), toolAllowsToolAdjust3(),
			deltaY);
		break;
	case CanvasShortcuts::COLOR_H_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorH), toolAllowsColorPick(),
			deltaY);
		break;
	case CanvasShortcuts::COLOR_S_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorS), toolAllowsColorPick(),
			deltaY);
		break;
	case CanvasShortcuts::COLOR_V_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorV), toolAllowsColorPick(),
			deltaY);
		break;
	case CanvasShortcuts::TOGGLE_ERASER:
	case CanvasShortcuts::TOGGLE_ERASE_MODE:
	case CanvasShortcuts::TOGGLE_RECOLOR_MODE:
	case CanvasShortcuts::UNDO:
	case CanvasShortcuts::REDO:
	case CanvasShortcuts::HIDE_DOCKS:
	case CanvasShortcuts::TRIGGER_ACTION:
		Q_EMIT canvasShortcutActionActivated(match.actionName());
		break;
	default:
		qWarning("Unhandled mouse wheel canvas shortcut %u", match.action());
		break;
	}
}

void CanvasControllerBase::wheelAdjust(
	QWheelEvent *event, int param, bool allowed, int delta)
{
	event->accept();
	if(allowed) {
		emit quickAdjust(param, qreal(delta) / 120.0, true);
	}
}

void CanvasControllerBase::handleKeyPress(QKeyEvent *event)
{
	int key = event->key();
	bool autoRepeat = event->isAutoRepeat();
	DP_EVENT_LOG(
		"key_press key=%d modifiers=0x%x autorepeat=%d", key,
		unsigned(event->modifiers()), int(autoRepeat));

	if(autoRepeat || m_hoveringOverHud) {
		return;
	}

	m_keysDown.insert(Qt::Key(key));

	Qt::KeyboardModifiers modifiers = getKeyboardModifiers(event);
	if(m_dragMode == ViewDragMode::Started && m_dragButton != Qt::NoButton) {
		// There's currently some dragging with a mouse button held down going
		// on. Switch to a different flavor of drag if appropriate and bail out.
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			modifiers, m_keysDown, m_dragButton);
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setUpdateOutline()
					.setResetCursor());
			break;
		default:
			break;
		}
		return;
	}

	if(m_penState == PenState::Up) {
		CanvasShortcuts::Match keyMatch =
			m_canvasShortcuts.matchKeyCombination(modifiers, Qt::Key(key));
		switch(keyMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromKeyMatch(keyMatch)
					.setPenMode(PenMode::Normal)
					.setDragMode(ViewDragMode::Started)
					.setResetDragPoints()
					.setResetDragRotation());
			break;
		case CanvasShortcuts::TOGGLE_ERASER:
		case CanvasShortcuts::TOGGLE_ERASE_MODE:
		case CanvasShortcuts::TOGGLE_RECOLOR_MODE:
		case CanvasShortcuts::UNDO:
		case CanvasShortcuts::REDO:
		case CanvasShortcuts::HIDE_DOCKS:
		case CanvasShortcuts::TRIGGER_ACTION:
			Q_EMIT canvasShortcutActionActivated(keyMatch.actionName());
			break;
		default:
			CanvasShortcuts::Match mouseMatch =
				m_canvasShortcuts.matchMouseButton(
					modifiers, m_keysDown, Qt::LeftButton);
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST1:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust1()));
				if(!toolAllowsToolAdjust1()) {
					emitPenModify(modifiers);
				}
				break;
			case CanvasShortcuts::TOOL_ADJUST2:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust2()));
				if(!toolAllowsToolAdjust2()) {
					emitPenModify(modifiers);
				}
				break;
			case CanvasShortcuts::TOOL_ADJUST3:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust3()));
				if(!toolAllowsToolAdjust3()) {
					emitPenModify(modifiers);
				}
				break;
			case CanvasShortcuts::COLOR_H_ADJUST:
			case CanvasShortcuts::COLOR_S_ADJUST:
			case CanvasShortcuts::COLOR_V_ADJUST:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsColorPick()));
				if(!toolAllowsColorPick()) {
					emitPenModify(modifiers);
				}
				break;
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			case CanvasShortcuts::CANVAS_ZOOM:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared));
				break;
			case CanvasShortcuts::COLOR_PICK:
				m_penMode = toolAllowsColorPick() ? PenMode::Colorpick
												  : PenMode::Normal;
				break;
			case CanvasShortcuts::LAYER_PICK:
				m_penMode = PenMode::Layerpick;
				break;
			default:
				m_penMode = PenMode::Normal;
				emitPenModify(modifiers);
				break;
			}
			break;
		}
	} else {
		emitPenModify(modifiers);
	}

	updateOutline();
	resetCursor();
}

void CanvasControllerBase::handleKeyRelease(QKeyEvent *event)
{
	bool autoRepeat = event->isAutoRepeat();
	int key = event->key();
	DP_EVENT_LOG(
		"key_release key=%d modifiers=0x%x autorepeat=%d", key,
		unsigned(event->modifiers()), int(autoRepeat));

	if(event->isAutoRepeat()) {
		return;
	}

	bool wasDragging = m_dragMode == ViewDragMode::Started;
	if(wasDragging) {
		CanvasShortcuts::Match dragMatch =
			m_dragButton == Qt::NoButton
				? m_canvasShortcuts.matchKeyCombination(
					  m_dragModifiers, Qt::Key(key))
				: m_canvasShortcuts.matchMouseButton(
					  m_dragModifiers, m_keysDown, m_dragButton);
		switch(dragMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			updateOutlinePos(mapPointToCanvasF(m_dragLastPoint));
			setDrag(
				SetDragParams::fromNone().setUpdateOutline().setResetCursor());
			break;
		default:
			break;
		}
	}

	m_keysDown.remove(Qt::Key(key));

	Qt::KeyboardModifiers modifiers = getKeyboardModifiers(event);
	if(wasDragging && m_dragButton != Qt::NoButton) {
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			modifiers, m_keysDown, m_dragButton);
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Started)
					.setResetDragRotation()
					.setUpdateOutline()
					.setResetCursor());
			break;
		default:
			break;
		}
		return;
	}

	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, Qt::LeftButton);
	if(m_dragMode == ViewDragMode::Prepared) {
		switch(mouseMatch.action()) {
		case CanvasShortcuts::TOOL_ADJUST1:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(
						toolAllowsToolAdjust1()));
			break;
		case CanvasShortcuts::TOOL_ADJUST2:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(
						toolAllowsToolAdjust2()));
			break;
		case CanvasShortcuts::TOOL_ADJUST3:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(
						toolAllowsToolAdjust3()));
			break;
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(
						toolAllowsColorPick()));
			break;
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared));
			break;
		default:
			setDrag(SetDragParams::fromNone());
			break;
		}
	}

	if(m_penState == PenState::Up) {
		switch(mouseMatch.action()) {
		case CanvasShortcuts::COLOR_PICK:
			m_penMode =
				toolAllowsColorPick() ? PenMode::Colorpick : PenMode::Normal;
			break;
		case CanvasShortcuts::LAYER_PICK:
			m_penMode = PenMode::Layerpick;
			break;
		default:
			m_penMode = PenMode::Normal;
			emitPenModify(modifiers);
			break;
		}
	} else {
		emitPenModify(modifiers);
	}

	updateOutline();
	resetCursor();
}

void CanvasControllerBase::init()
{
	TouchHandler *touch = touchHandler();
	connect(
		touch, &TouchHandler::touchPressed, this,
		&CanvasControllerBase::touchPressEvent, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchMoved, this,
		&CanvasControllerBase::touchMoveEvent, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchReleased, this,
		&CanvasControllerBase::touchReleaseEvent, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchScrolledBy, this,
		&CanvasControllerBase::scrollByF, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchZoomedRotated, this,
		&CanvasControllerBase::touchZoomRotate, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchTapActionActivated, this,
		&CanvasControllerBase::touchTapActionActivated, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchColorPicked, this,
		&CanvasControllerBase::touchColorPick, Qt::DirectConnection);
	connect(
		touch, &TouchHandler::touchColorPickFinished, this,
		&CanvasControllerBase::finishColorPick, Qt::DirectConnection);

	resetCanvasTransform();
}

QRect CanvasControllerBase::canvasRect() const
{
	return QRect(QPoint(0, 0), m_canvasSize);
}

QRectF CanvasControllerBase::canvasRectF() const
{
	return QRectF(canvasRect());
}

QPointF CanvasControllerBase::viewCenterF() const
{
	return viewRectF().center();
}

QRectF CanvasControllerBase::viewRectF() const
{
	return QRectF(QPointF(0.0, 0.0), viewSizeF());
}

QSizeF CanvasControllerBase::viewSizeF() const
{
	return QSizeF(viewSize());
}

void CanvasControllerBase::setClearColor(const QColor clearColor)
{
	if(clearColor != m_clearColor) {
		m_clearColor = clearColor;
		emit clearColorChanged();
	}
}

void CanvasControllerBase::setRenderSmooth(bool renderSmooth)
{
	if(renderSmooth != m_renderSmooth) {
		m_renderSmooth = renderSmooth;
		emit renderSmoothChanged();
	}
}

void CanvasControllerBase::setTabletEnabled(bool tabletEnabled)
{
	m_tabletEnabled = tabletEnabled;
}

void CanvasControllerBase::setSerializedPressureCurve(
	const QString &serializedPressureCurve)
{
	KisCubicCurve pressureCurve;
	pressureCurve.fromString(serializedPressureCurve);
	m_pressureCurve = pressureCurve;
}

void CanvasControllerBase::setSerializedPressureCurveEraser(
	const QString &serializedPressureCurveEraser)
{
	KisCubicCurve pressureCurveEraser;
	pressureCurveEraser.fromString(serializedPressureCurveEraser);
	m_pressureCurveEraser = pressureCurveEraser;
}

void CanvasControllerBase::setPressureCurveMode(int pressureCurveMode)
{
	m_pressureCurveMode = pressureCurveMode;
}

void CanvasControllerBase::setOutlineWidth(qreal outlineWidth)
{
	if(outlineWidth != m_outlineWidth) {
		changeOutline([&]() {
			m_outlineWidth = outlineWidth;
		});
	}
}

void CanvasControllerBase::setCanvasShortcuts(QVariantMap canvasShortcuts)
{
	m_canvasShortcuts = CanvasShortcuts::load(canvasShortcuts);
}

void CanvasControllerBase::updatePixelGridScale()
{
	qreal pixelGridScale = m_pixelGrid && m_zoom >= 8.0 ? m_zoom : 0.0;
	if(pixelGridScale != m_pixelGridScale) {
		m_pixelGridScale = pixelGridScale;
		emit pixelGridScaleChanged();
	}
}

void CanvasControllerBase::setLocked(
	bool locked, const QStringList &descriptions,
	const QVector<QAction *> &actions)
{
	if(locked != m_locked) {
		m_locked = locked;
	}

	if(m_lockDescriptions != descriptions || m_lockActions != actions) {
		m_lockDescriptions = descriptions;
		m_lockActions = actions;
		updateLockNotice();
		updateOutline();
		clearHudHover();
	}
}

void CanvasControllerBase::setToolState(int toolState)
{
	if(toolState != m_toolState) {
		m_toolState = toolState;
		updateOutline();
		resetCursor();
	}
}

void CanvasControllerBase::setSaveInProgress(bool saveInProgress)
{
	if(saveInProgress != m_saveInProgress) {
		m_saveInProgress = saveInProgress;
		emit saveInProgressChanged(saveInProgress);
		updateLockNotice();
	}
}

void CanvasControllerBase::setOutlineSize(int outlineSize)
{
	if(outlineSize != m_outlineSize) {
		changeOutline([&]() {
			m_outlineSize = outlineSize;
		});
	}
}

void CanvasControllerBase::setOutlineMode(
	bool subpixel, bool square, bool force)
{
	if(subpixel != m_subpixelOutline || square != m_squareOutline ||
	   force != m_forceOutline) {
		changeOutline([&]() {
			m_subpixelOutline = subpixel;
			m_squareOutline = square;
			m_forceOutline = force;
		});
	}
}

void CanvasControllerBase::setOutlineOffset(const QPointF &outlineOffset)
{
	if(outlineOffset != m_outlineOffset) {
		changeOutline([&] {
			m_outlineOffset = outlineOffset;
		});
	}
}

void CanvasControllerBase::setPixelGrid(bool pixelGrid)
{
	if(pixelGrid != m_pixelGrid) {
		m_pixelGrid = pixelGrid;
		updatePixelGridScale();
	}
}

void CanvasControllerBase::setPointerTracking(bool pointerTracking)
{
	m_pointerTracking = pointerTracking;
}

void CanvasControllerBase::setToolCapabilities(unsigned int toolCapabilities)
{
	m_toolCapabilities = tools::Capabilities(toolCapabilities);
	touchHandler()->setAllowColorPick(toolAllowsColorPick());
}

void CanvasControllerBase::setToolCursor(const QCursor &toolCursor)
{
	if(toolCursor != m_toolCursor) {
		m_toolCursor = toolCursor;
		resetCursor();
	}
}

void CanvasControllerBase::setBrushBlendMode(int brushBlendMode)
{
	if(brushBlendMode != m_brushBlendMode) {
		m_brushBlendMode = brushBlendMode;
		resetCursor();
	}
}

#if defined(__EMSCRIPTEN__) || defined(Q_OS_ANDROID)
void CanvasControllerBase::setEnableEraserOverride(bool enableEraserOverride)
{
	m_enableEraserOverride = enableEraserOverride;
}
#endif

bool CanvasControllerBase::isOutlineVisible() const
{
	return m_showOutline && m_outlineVisibleInMode && m_outlineSize > 0.0 &&
		   m_outlineWidth > 0.0;
}

QPointF CanvasControllerBase::outlinePos() const
{
	QPointF outlinePos = m_outlinePos + m_outlineOffset;
	if(m_subpixelOutline) {
		return outlinePos;
	} else {
		QPointF pixelPos(
			std::floor(outlinePos.x()), std::floor(outlinePos.y()));
		if(m_outlineSize % 2 == 0) {
			return pixelPos;
		} else {
			return pixelPos + QPointF(0.5, 0.5);
		}
	}
}

void CanvasControllerBase::setShowTransformNotices(bool showTransformNotices)
{
	m_showTransformNotices = showTransformNotices;
}

void CanvasControllerBase::setTabletEventTimerDelay(int tabletEventTimerDelay)
{
	m_tabletEventTimerDelay = tabletEventTimerDelay;
	m_tabletEventTimer.setRemainingTime(0);
}

void CanvasControllerBase::resetTabletDriver()
{
	m_eraserTipActive = false;
	resetTabletFilter();
}

void CanvasControllerBase::setEraserTipActive(bool eraserTipActive)
{
	m_eraserTipActive = eraserTipActive;
}

void CanvasControllerBase::setBrushCursorStyle(int brushCursorStyle)
{
	if(brushCursorStyle != m_brushCursorStyle) {
		m_brushCursorStyle = brushCursorStyle;
		resetCursor();
	}
}

void CanvasControllerBase::setEraseCursorStyle(int eraseCursorStyle)
{
	if(eraseCursorStyle != m_eraseCursorStyle) {
		m_eraseCursorStyle = eraseCursorStyle;
		resetCursor();
	}
}

void CanvasControllerBase::setAlphaLockCursorStyle(int alphaLockCursorStyle)
{
	if(alphaLockCursorStyle != m_alphaLockCursorStyle) {
		m_alphaLockCursorStyle = alphaLockCursorStyle;
		resetCursor();
	}
}

void CanvasControllerBase::clearHudHover()
{
	m_hoveringOverHud = false;
	removeHudHover();
	resetCursor();
}

void CanvasControllerBase::startTabletEventTimer()
{
	if(m_tabletEventTimerDelay > 0) {
		m_tabletEventTimer.setRemainingTime(m_tabletEventTimerDelay);
	}
}

void CanvasControllerBase::penMoveEvent(
	long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::KeyboardModifiers modifiers)
{
	if(m_hudActionToActivate.isValid()) {
		return;
	}

	canvas::Point point =
		mapPenPointToCanvasF(timeMsec, posf, pressure, xtilt, ytilt, rotation);
	emit coordinatesChanged(point);

	setCursorPos(posf);

	if(m_dragMode == ViewDragMode::Started) {
		moveDrag(posf.toPoint());
	} else {
		if(m_prevPoint.isDifferent(
			   point, toolFractional(), toolSnapsToPixel())) {
			if(m_penState == PenState::Up) {
				CanvasShortcuts::ConstraintMatch match =
					m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
				emit penHover(
					point, m_rotation, m_zoom / devicePixelRatioF(), m_mirror,
					m_flip, match.toolConstraint1(), match.toolConstraint2());
				if(m_pointerTracking && m_canvasModel) {
					emit pointerMove(point);
				}

				HudAction action = checkHudHover(posf);
				m_hoveringOverHud = action.isValid();
				if(m_hoveringOverHud != action.wasHovering) {
					updateOutline();
					resetCursor();
				}
			} else {
				m_pointerVelocity = point.distance(m_prevPoint);
				m_pointerDistance += m_pointerVelocity;
				if(m_canvasModel) {
					CanvasShortcuts::ConstraintMatch match =
						m_canvasShortcuts.matchConstraints(
							modifiers, m_keysDown);
					switch(m_penMode) {
					case PenMode::Normal:
						if(!m_locked || toolSendsNoMessages()) {
							emit penMove(
								point.timeMsec(), point, point.pressure(),
								point.xtilt(), point.ytilt(), point.rotation(),
								match.toolConstraint1(),
								match.toolConstraint2(), posf);
						}
						break;
					case PenMode::Colorpick:
						pickColor(
							int(tools::ColorPickSource::Canvas), point, posf);
						break;
					case PenMode::Layerpick:
						m_canvasModel->pickLayer(point.x(), point.y());
						break;
					default:
						break;
					}
				}
			}
			m_prevPoint = point;
		}
		updateOutlinePos(point);
	}
}

void CanvasControllerBase::penPressEvent(
	long long timeMsec, const QPointF &posf, const QPoint &globalPos,
	qreal pressure, qreal xtilt, qreal ytilt, qreal rotation,
	Qt::MouseButton button, Qt::KeyboardModifiers modifiers, int deviceType,
	bool eraserOverride)
{
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	m_eraserTipActive = eraserOverride;
#endif

	if(m_hudActionToActivate.isValid()) {
		return;
	}

	if(m_penState == PenState::Up) {
		HudAction action = checkHudHover(posf.toPoint());
		m_hoveringOverHud = action.isValid();
		if(m_hoveringOverHud != action.wasHovering) {
			updateOutline();
			resetCursor();
		}
		if(m_hoveringOverHud) {
			m_hudActionToActivate = action;
			m_hudActionGlobalPos = globalPos;
			m_hudActionDeviceType = deviceType;
			// Some users operate context menus by pressing down, moving the
			// cursor over the intended action and then releasing. With touch,
			// you don't want this to happen, since it just ends up accidentally
			// activating whatever was under the finger. On Windows, we will get
			// a garbage mouse press right after the tablet press, so we open
			// the menu in handleMousePress, otherwise it will instantly close.
			if(m_hudActionToActivate.type == HudAction::Type::TriggerMenu &&
			   deviceType != int(tools::DeviceType::Touch)
#ifdef Q_OS_WINDOWS
			   && deviceType != int(tools::DeviceType::Tablet)
#endif
			) {
				activatePendingHudAction();
			}
			return;
		}

		CanvasShortcuts::Match match =
			m_canvasShortcuts.matchMouseButton(modifiers, m_keysDown, button);
		// If the tool wants to receive right clicks (e.g. to undo the last
		// point in a bezier curve), we have to override that shortcut.
		if(toolHandlesRightClick() &&
		   match.isUnmodifiedClick(Qt::RightButton)) {
			match.shortcut = nullptr;
		}

		PenMode penMode = PenMode::Normal;

		switch(match.action()) {
		case CanvasShortcuts::NO_ACTION:
			break;
		case CanvasShortcuts::TOOL_ADJUST1:
			setDrag(
				SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(
						toolAllowsToolAdjust1() &&
						m_dragMode != ViewDragMode::Started)
					.setUpdateOutline());
			break;
		case CanvasShortcuts::TOOL_ADJUST2:
			setDrag(
				SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(
						toolAllowsToolAdjust2() &&
						m_dragMode != ViewDragMode::Started)
					.setUpdateOutline());
			break;
		case CanvasShortcuts::TOOL_ADJUST3:
			setDrag(
				SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(
						toolAllowsToolAdjust3() &&
						m_dragMode != ViewDragMode::Started)
					.setUpdateOutline());
			break;
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(
						toolAllowsColorPick() &&
						m_dragMode != ViewDragMode::Started));
			break;
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
			setDrag(
				SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(m_dragMode != ViewDragMode::Started));
			break;
		case CanvasShortcuts::COLOR_PICK:
			if(toolAllowsColorPick()) {
				penMode = PenMode::Colorpick;
			}
			break;
		case CanvasShortcuts::LAYER_PICK:
			penMode = PenMode::Layerpick;
			break;
		case CanvasShortcuts::TOGGLE_ERASER:
		case CanvasShortcuts::TOGGLE_ERASE_MODE:
		case CanvasShortcuts::TOGGLE_RECOLOR_MODE:
		case CanvasShortcuts::UNDO:
		case CanvasShortcuts::REDO:
		case CanvasShortcuts::HIDE_DOCKS:
		case CanvasShortcuts::TRIGGER_ACTION:
			Q_EMIT canvasShortcutActionActivated(match.actionName());
			break;
		default:
			qWarning(
				"Unhandled mouse button canvas shortcut %u", match.action());
			break;
		}

		if(m_dragMode == ViewDragMode::Prepared) {
			m_dragLastPoint = posf.toPoint();
			m_dragCanvasPoint = mapPointToCanvas(m_dragLastPoint);
			setDrag(SetDragParams(
						m_dragAction, button, modifiers, m_dragInverted,
						m_dragSwapAxes, int(ViewDragMode::Started))
						.setResetDragRotation()
						.setResetCursor());
		} else if(
			(button == Qt::LeftButton || button == Qt::RightButton ||
			 penMode != PenMode::Normal) &&
			m_dragMode == ViewDragMode::None) {
			m_penState = deviceType == int(tools::DeviceType::Tablet)
							 ? PenState::TabletDown
							 : PenState::MouseDown;
			m_pointerDistance = 0;
			m_pointerVelocity = 0;
			canvas::Point point = mapPenPointToCanvasF(
				timeMsec, posf, pressure, xtilt, ytilt, rotation);
			m_prevPoint = point;

			if(penMode != m_penMode) {
				m_penMode = penMode;
				resetCursor();
			}

			if(m_canvasModel) {
				switch(m_penMode) {
				case PenMode::Normal:
					if(!m_locked || toolSendsNoMessages()) {
						CanvasShortcuts::ConstraintMatch constraintMatch =
							m_canvasShortcuts.matchConstraints(
								modifiers, m_keysDown);
						emit penDown(
							point.timeMsec(), point, point.pressure(),
							point.xtilt(), point.ytilt(), point.rotation(),
							button == Qt::RightButton, m_rotation,
							m_zoom / devicePixelRatioF(), m_mirror, m_flip,
							constraintMatch.toolConstraint1(),
							constraintMatch.toolConstraint2(), posf, deviceType,
							eraserOverride);
					}
					break;
				case PenMode::Colorpick:
					pickColor(int(tools::ColorPickSource::Canvas), point, posf);
					break;
				case PenMode::Layerpick:
					m_canvasModel->pickLayer(point.x(), point.y());
					break;
				}
			}
		}
	}
}

void CanvasControllerBase::penReleaseEvent(
	long long timeMsec, const QPointF &posf, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers)
{
	activatePendingHudAction();

	canvas::Point point =
		mapPenPointToCanvasF(timeMsec, posf, 0.0, 0.0, 0.0, 0.0);
	m_prevPoint = point;
	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, Qt::LeftButton);

	if(m_dragMode != ViewDragMode::None && m_dragButton != Qt::NoButton) {
		switch(mouseMatch.action()) {
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared));
			break;
		default:
			setDrag(SetDragParams::fromNone());
			break;
		}

	} else if(
		m_penState == PenState::TabletDown ||
		((button == Qt::LeftButton || button == Qt::RightButton) &&
		 m_penState == PenState::MouseDown)) {

		if((!m_locked || toolSendsNoMessages()) &&
		   m_penMode == PenMode::Normal) {
			CanvasShortcuts::ConstraintMatch constraintMatch =
				m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
			emit penUp(
				constraintMatch.toolConstraint1(),
				constraintMatch.toolConstraint2());
		}

		if(m_pickingColor) {
			finishColorPick();
		}

		m_penState = PenState::Up;

		m_hoveringOverHud = checkHudHover(posf.toPoint()).isValid();
		if(!m_hoveringOverHud) {
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST1:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust1()));
				break;
			case CanvasShortcuts::TOOL_ADJUST2:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust2()));
				break;
			case CanvasShortcuts::TOOL_ADJUST3:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsToolAdjust3()));
				break;
			case CanvasShortcuts::COLOR_H_ADJUST:
			case CanvasShortcuts::COLOR_S_ADJUST:
			case CanvasShortcuts::COLOR_V_ADJUST:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(toolAllowsColorPick()));
				break;
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			case CanvasShortcuts::CANVAS_ZOOM:
				setDrag(
					SetDragParams::fromMouseMatch(mouseMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Prepared));
				break;
			case CanvasShortcuts::COLOR_PICK:
				if(toolAllowsColorPick()) {
					m_penMode = PenMode::Colorpick;
				}
				break;
			case CanvasShortcuts::LAYER_PICK:
				m_penMode = PenMode::Layerpick;
				break;
			default:
				m_penMode = PenMode::Normal;
				break;
			}
		}
	}

	setOutlinePos(point);
	updateOutline();
	resetCursor();
}

void CanvasControllerBase::touchPressEvent(
	QEvent *event, long long timeMsec, const QPointF &posf,
	const QPoint &globalPos, qreal pressure)
{
	Q_UNUSED(event);
	penPressEvent(
		timeMsec, posf, globalPos, pressure, 0.0, 0.0, 0.0, Qt::LeftButton,
		Qt::NoModifier, int(tools::DeviceType::Touch), false);
}

void CanvasControllerBase::touchMoveEvent(
	long long timeMsec, const QPointF &posf, qreal pressure)
{
	penMoveEvent(timeMsec, posf, pressure, 0.0, 0.0, 0.0, Qt::NoModifier);
}

void CanvasControllerBase::touchReleaseEvent(
	long long timeMsec, const QPointF &posf)
{
	penReleaseEvent(timeMsec, posf, Qt::LeftButton, Qt::NoModifier);
}

void CanvasControllerBase::touchZoomRotate(qreal zoom, qreal rotation)
{
	{
		QScopedValueRollback<bool> rollback(m_blockNotices, true);
		setZoom(zoom);
		setRotation(rotation);
	}
	showTouchTransformNotice();
}

void CanvasControllerBase::setDrag(const SetDragParams &params)
{
	ViewDragMode prevMode = m_dragMode;
	CanvasShortcuts::Action prevAction = m_dragAction;

	if(params.hasPenMode()) {
		m_penMode = params.penMode();
	}

	if(params.isDragConditionFulfilled()) {
		bool shouldSetDragParams = true;
		if(params.hasDragMode()) {
			m_dragMode = params.dragMode();
			if(m_dragMode == ViewDragMode::None) {
				shouldSetDragParams = false;
			}
		}

		if(shouldSetDragParams) {
			m_dragAction = params.action();
			m_dragButton = params.button();
			m_dragModifiers = params.modifiers();
			m_dragInverted = params.inverted();
			m_dragSwapAxes = params.swapAxes();

			if(params.resetDragPoints()) {
				m_dragLastPoint = mapPointFromGlobal(QCursor::pos());
				m_dragCanvasPoint = mapPointToCanvasF(m_dragLastPoint);
			}

			if(params.resetDragRotation()) {
				m_dragDiscreteRotation = 0.0;
				m_dragSnapRotation = rotation();
			}
		}

		if(params.updateOutline()) {
			updateOutline();
		}

		if(params.resetCursor()) {
			resetCursor();
		}
	}

	if(m_dragMode == ViewDragMode::Started) {
		if(prevMode != ViewDragMode::Started &&
		   CanvasShortcuts::isColorAdjustAction(m_dragAction)) {
			showColorPick(int(tools::ColorPickSource::Adjust), m_dragLastPoint);
		}
	} else if(
		prevMode == ViewDragMode::Started &&
		CanvasShortcuts::isColorAdjustAction(prevAction)) {
		hideColorPick();
	}
}

void CanvasControllerBase::moveDrag(const QPoint &point)
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
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
	case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP: {
		QSizeF halfSize = viewSizeF() / 2.0;
		qreal hw = halfSize.width();
		qreal hh = halfSize.height();
		qreal a1 = qAtan2(hw - m_dragLastPoint.x(), hh - m_dragLastPoint.y());
		qreal a2 = qAtan2(hw - point.x(), hh - point.y());
		qreal ad = qRadiansToDegrees(a1 - a2);
		qreal r = m_dragInverted ? -ad : ad;
		if(m_dragAction == CanvasShortcuts::CANVAS_ROTATE) {
			m_dragSnapRotation += r;
			setRotationSnap(m_dragSnapRotation);
		} else if(m_dragAction == CanvasShortcuts::CANVAS_ROTATE_DISCRETE) {
			m_dragDiscreteRotation += r;
			qreal discrete = m_dragDiscreteRotation / DISCRETE_ROTATION_STEP;
			if(discrete >= 1.0) {
				int steps = qFloor(discrete);
				m_dragDiscreteRotation -= steps * DISCRETE_ROTATION_STEP;
				rotateByDiscreteSteps(steps);
			} else if(discrete <= -1.0) {
				int steps = qCeil(discrete);
				m_dragDiscreteRotation += steps * -DISCRETE_ROTATION_STEP;
				rotateByDiscreteSteps(steps);
			}
		} else {
			setRotation(rotation() + r);
		}
		break;
	}
	case CanvasShortcuts::CANVAS_ZOOM:
		if(deltaY != 0) {
			qreal delta = qBound(-1.0, deltaY / 100.0, 1.0);
			if(delta > 0.0) {
				setZoomAt(m_zoom * (1.0 + delta), m_dragCanvasPoint);
			} else if(delta < 0.0) {
				setZoomAt(m_zoom / (1.0 - delta), m_dragCanvasPoint);
			}
		}
		break;
	case CanvasShortcuts::TOOL_ADJUST1:
		dragAdjust(int(tools::QuickAdjustType::Tool1), deltaX, 1.2);
		break;
	case CanvasShortcuts::TOOL_ADJUST2:
		dragAdjust(int(tools::QuickAdjustType::Tool2), deltaX, 1.2);
		break;
	case CanvasShortcuts::TOOL_ADJUST3:
		dragAdjust(int(tools::QuickAdjustType::Tool3), deltaX, 1.2);
		break;
	case CanvasShortcuts::COLOR_H_ADJUST:
		dragAdjust(int(tools::QuickAdjustType::ColorH), deltaX, 1.0);
		break;
	case CanvasShortcuts::COLOR_S_ADJUST:
		dragAdjust(int(tools::QuickAdjustType::ColorS), deltaX, 1.0);
		break;
	case CanvasShortcuts::COLOR_V_ADJUST:
		dragAdjust(int(tools::QuickAdjustType::ColorV), deltaX, 1.0);
		break;
	default:
		qWarning("Unhandled drag action %u", m_dragAction);
		break;
	}

	m_dragLastPoint = point;
}

void CanvasControllerBase::dragAdjust(int type, int delta, qreal acceleration)
{
	// Horizontally, dragging right (+X) is higher and left (-X) is lower,
	// but vertically, dragging up (-Y) is higher and down (+Y) is lower.
	// We have to invert in one of those cases to match with that logic.
	qreal d = qreal(m_dragSwapAxes ? delta : -delta);
	if(acceleration != -1) {
		d = std::copysign(std::pow(std::abs(d), acceleration), d);
	}
	emit quickAdjust(type, d * 0.1, false);
}

void CanvasControllerBase::pickColor(
	int source, const QPointF &point, const QPointF &posf)
{
	m_canvasModel->pickColor(point.x(), point.y(), 0, 0);
	showColorPick(source, posf);
	m_pickingColor = true;
}

void CanvasControllerBase::touchColorPick(const QPointF &posf)
{
	pickColor(
		int(tools::ColorPickSource::Touch), mapPointToCanvasF(posf), posf);
}

void CanvasControllerBase::finishColorPick()
{
	if(m_canvasModel) {
		m_canvasModel->finishColorPick();
	}
	hideColorPick();
	m_pickingColor = false;
}

void CanvasControllerBase::resetCursor()
{
	if(m_dragMode != ViewDragMode::None) {
		switch(m_dragAction) {
		case CanvasShortcuts::CANVAS_PAN:
			setViewportCursor(
				m_dragMode == ViewDragMode::Started ? Qt::ClosedHandCursor
													: Qt::OpenHandCursor);
			break;
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			setViewportCursor(Cursors::rotate());
			break;
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			setViewportCursor(Cursors::rotateDiscrete());
			break;
		case CanvasShortcuts::CANVAS_ZOOM:
			setViewportCursor(Cursors::zoom());
			break;
		case CanvasShortcuts::TOOL_ADJUST1:
		case CanvasShortcuts::TOOL_ADJUST2:
		case CanvasShortcuts::TOOL_ADJUST3:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setViewportCursor(
				m_dragSwapAxes ? Qt::SplitVCursor : Qt::SplitHCursor);
			break;
		default:
			setViewportCursor(Qt::ForbiddenCursor);
			break;
		}
		return;
	}

	if(m_hoveringOverHud) {
		setViewportCursor(Qt::PointingHandCursor);
		return;
	}

	switch(m_penMode) {
	case PenMode::Normal:
		break;
	case PenMode::Colorpick:
		setViewportCursor(Cursors::colorPick());
		return;
	case PenMode::Layerpick:
		setViewportCursor(Cursors::layerPick());
		return;
	}

	if(m_locked && !toolSendsNoMessages()) {
		setViewportCursor(Qt::ForbiddenCursor);
		return;
	} else if(m_toolState == int(tools::ToolState::Busy)) {
		setViewportCursor(Qt::WaitCursor);
		return;
	}

	if(m_toolCursor.shape() == Qt::CrossCursor) {
		switch(getCurrentCursorStyle()) {
		case int(view::Cursor::Dot):
			setViewportCursor(Cursors::dot());
			break;
		case int(view::Cursor::Cross):
			setViewportCursor(Qt::CrossCursor);
			break;
		case int(view::Cursor::Arrow):
			setViewportCursor(Qt::ArrowCursor);
			break;
		case int(view::Cursor::TriangleLeft):
			setViewportCursor(Cursors::triangleLeft());
			break;
		case int(view::Cursor::TriangleRight):
			setViewportCursor(Cursors::triangleRight());
			break;
		case int(view::Cursor::Eraser):
			setViewportCursor(Cursors::eraser());
			break;
		case int(view::Cursor::Blank):
			setViewportCursor(Qt::BlankCursor);
			break;
		default:
			setViewportCursor(Cursors::triangleRight());
			break;
		}
	} else {
		setViewportCursor(m_toolCursor);
	}
}

int CanvasControllerBase::getCurrentCursorStyle() const
{
	if(canvas::blendmode::presentsAsEraser(m_brushBlendMode)) {
		if(m_eraseCursorStyle != int(view::Cursor::SameAsBrush)) {
			return m_eraseCursorStyle;
		}
	} else if(canvas::blendmode::preservesAlpha(m_brushBlendMode)) {
		if(m_alphaLockCursorStyle != int(view::Cursor::SameAsBrush)) {
			return m_alphaLockCursorStyle;
		}
	}
	return m_brushCursorStyle;
}

void CanvasControllerBase::updateOutlinePos(QPointF point)
{
	if(canvas::Point::isOutlinePosDifferent(
		   point, m_outlinePos, m_subpixelOutline)) {
		setOutlinePos(point);
		if(isOutlineVisible()) {
			emit outlineChanged();
		}
	}
}

void CanvasControllerBase::setOutlinePos(QPointF point)
{
	m_outlinePos = point;
}

void CanvasControllerBase::updateOutline()
{
	updateOutlineVisibleInMode();
	emit outlineChanged();
}

void CanvasControllerBase::updateOutlineVisibleInMode()
{
	m_outlineVisibleInMode =
		!m_hoveringOverHud &&
		(m_dragMode == ViewDragMode::None
			 ? m_penMode == PenMode::Normal && !m_locked &&
				   m_toolState == int(tools::ToolState::Normal)
			 : m_dragAction == CanvasShortcuts::TOOL_ADJUST1);
}

QPointF CanvasControllerBase::getOutlinePos() const
{
	QPointF pos = mapPointFromCanvasF(m_outlinePos);
	if(!m_subpixelOutline && m_outlineSize % 2 == 0) {
		qreal offset = actualZoom() * 0.5;
		pos -= QPointF(offset, offset);
	}
	return pos;
}

qreal CanvasControllerBase::getOutlineWidth() const
{
	return m_forceOutline ? qMax(1.0, m_outlineWidth) : m_outlineWidth;
}

void CanvasControllerBase::changeOutline(const std::function<void()> &block)
{
	bool wasVisible = isOutlineVisible();
	block();
	bool isVisible = isOutlineVisible();
	if(isVisible || wasVisible != isVisible) {
		emit outlineChanged();
	}
}

QPointF CanvasControllerBase::mousePosF(QMouseEvent *event) const
{
	return compat::mousePosition(*event);
}

QPointF CanvasControllerBase::tabletPosF(QTabletEvent *event) const
{
	return compat::tabPosF(*event);
}

QPointF CanvasControllerBase::wheelPosF(QWheelEvent *event) const
{
	return compat::wheelPosition(*event);
}

bool CanvasControllerBase::isSynthetic(QMouseEvent *event)
{
	return event->source() & Qt::MouseEventSynthesizedByQt;
}

bool CanvasControllerBase::isSyntheticTouch(QMouseEvent *event)
{
#ifdef Q_OS_WIN
	// Windows may generate bogus mouse events from touches, we never want this.
	return event->source() & Qt::MouseEventSynthesizedBySystem;
#else
	Q_UNUSED(event);
	return false;
#endif
}

#ifdef DP_CANVAS_CONTROLLER_HOVER_EVENTS
bool CanvasControllerBase::isSyntheticHover(QHoverEvent *event)
{
	Q_UNUSED(event);
	// TODO: Figure this one out on Windows.
	return false;
}
#endif

Qt::KeyboardModifiers
CanvasControllerBase::getKeyboardModifiers(const QKeyEvent *event) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(event);
	return getFallbackModifiers();
#else
	return event->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasControllerBase::getMouseModifiers(const QMouseEvent *event) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(event);
	return getFallbackModifiers();
#else
	return event->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasControllerBase::getTabletModifiers(const QTabletEvent *event) const
{
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	// Qt always reports no modifiers on Android.
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(event);
	return getFallbackModifiers();
#else
#	ifdef Q_OS_LINUX
	// Tablet event modifiers aren't reported properly on Wayland.
	if(m_waylandWorkarounds) {
		return getFallbackModifiers();
	}
#	endif
	return event->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasControllerBase::getWheelModifiers(const QWheelEvent *event) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(event);
	return getFallbackModifiers();
#else
	return event->modifiers();
#endif
}

Qt::KeyboardModifiers CanvasControllerBase::getFallbackModifiers() const
{
	Qt::KeyboardModifiers mods;
	mods.setFlag(Qt::ControlModifier, m_keysDown.contains(Qt::Key_Control));
	mods.setFlag(Qt::ShiftModifier, m_keysDown.contains(Qt::Key_Shift));
	mods.setFlag(Qt::AltModifier, m_keysDown.contains(Qt::Key_Alt));
	mods.setFlag(Qt::MetaModifier, m_keysDown.contains(Qt::Key_Meta));
	return mods;
}

canvas::Point CanvasControllerBase::mapPenPointToCanvasF(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation) const
{
	return canvas::Point(
		timeMsec, mapPointToCanvasF(point), mapPressure(pressure), xtilt, ytilt,
		rotation);
}

qreal CanvasControllerBase::mapPressure(qreal pressure) const
{
	if(m_pressureCurveMode && m_eraserTipActive) {
		return m_pressureCurveEraser.value(pressure);
	} else {
		return m_pressureCurve.value(pressure);
	}
}

QPointF CanvasControllerBase::mapPointFromCanvasF(const QPointF &point) const
{
	return m_transform.map(point);
}

QPolygonF CanvasControllerBase::mapRectFromCanvas(const QRect &rect) const
{
	return m_transform.map(QRectF(rect));
}

QPointF CanvasControllerBase::mapPointToCanvas(const QPoint &point) const
{
	return mapPointToCanvasF(QPointF(point));
}

QPointF CanvasControllerBase::mapPointToCanvasF(const QPointF &point) const
{
	return m_invertedTransform.map(point + viewToCanvasOffset());
}

QPolygonF CanvasControllerBase::mapRectToCanvasF(const QRectF &rect) const
{
	return m_invertedTransform.map(rect.translated(viewToCanvasOffset()));
}

void CanvasControllerBase::updateCanvasTransform(
	const std::function<void()> &block)
{
	block();
	updatePosBounds();
	resetCanvasTransform();
}

void CanvasControllerBase::resetCanvasTransform()
{
	m_transform = calculateCanvasTransform();
	m_invertedTransform = m_transform.inverted();
	updateSceneTransform(calculateCanvasTransformFrom(
		m_pos + viewToCanvasOffset(), m_zoom, m_rotation, m_mirror, m_flip));
	emitScrollAreaChanged();
	emitTransformChanged();
	emitViewRectChanged();
	updatePixelGridScale();
}

void CanvasControllerBase::updatePosBounds()
{
	QPointF vc = viewCenterF();
	QTransform matrix = calculateCanvasTransformFrom(
		vc + viewToCanvasOffset(), m_zoom, m_rotation, m_mirror, m_flip);
	qreal hmargin = vc.x() - 64.0;
	qreal vmargin = vc.y() - 64.0;
	m_posBounds =
		matrix.map(canvasRectF())
			.boundingRect()
			.marginsAdded(QMarginsF(hmargin, vmargin, hmargin, vmargin));
	clampPos();
}

void CanvasControllerBase::clampPos()
{
	if(m_posBounds.isValid()) {
		m_pos.setX(qBound(m_posBounds.left(), m_pos.x(), m_posBounds.right()));
		m_pos.setY(qBound(m_posBounds.top(), m_pos.y(), m_posBounds.bottom()));
	}
}

QTransform CanvasControllerBase::calculateCanvasTransformFrom(
	const QPointF &pos, qreal zoom, qreal rotation, bool mirror,
	bool flip) const
{
	QTransform matrix;
	matrix.translate(-std::round(pos.x()), -std::round(pos.y()));
	qreal scale = actualZoomFor(zoom);
	matrix.scale(scale, scale);
	mirrorFlip(matrix, mirror, flip);
	matrix.rotate(rotation);
	return matrix;
}

void CanvasControllerBase::mirrorFlip(
	QTransform &matrix, bool mirror, bool flip)
{
	matrix.scale(mirror ? -1.0 : 1.0, flip ? -1.0 : 1.0);
}

void CanvasControllerBase::setZoomToFit(Qt::Orientations orientations)
{
	QSize widgetSize = viewSize();
	if(!widgetSize.isEmpty()) {
		qreal dpr = devicePixelRatioF();
		QRectF r = QRectF(QPointF(0.0, 0.0), QSizeF(m_canvasSize) / dpr);
		qreal xScale = qreal(widgetSize.width()) / r.width();
		qreal yScale = qreal(widgetSize.height()) / r.height();
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

		QPointF center = r.center();
		qreal rotation = m_rotation;
		bool mirror = m_mirror;
		bool flip = m_flip;

		m_pos = center - viewTransformOffset();
		m_zoom = 1.0;
		m_rotation = 0.0;
		m_mirror = false;
		m_flip = false;

		{
			QSignalBlocker blocker(this);
			QScopedValueRollback<bool> rollback(m_blockNotices, true);
			qreal newZoom = qBound(getZoomMin(), scale, getZoomMax());
			updateCanvasTransform([&] {
				m_pos += center * (newZoom - m_zoom);
				m_zoom = newZoom;
			});
			setRotation(rotation);
			setMirror(mirror);
			setFlip(flip);
		}

		showTransformNotice(getZoomNoticeText());
		emitTransformChanged();
	}
}

void CanvasControllerBase::setRotationSnap(qreal degrees)
{
	setRotation(qAbs(std::fmod(degrees, 360.0)) < 5.0 ? 0.0 : degrees);
}

void CanvasControllerBase::rotateByDiscreteSteps(int steps)
{
	constexpr qreal EPS = 0.01;
	constexpr qreal FULL = DISCRETE_ROTATION_STEP;
	constexpr qreal HALF = FULL / 2.0;

	// If we're not close to a discrete position, snap to it first.
	qreal offset = std::fmod(m_rotation, FULL);
	if(steps < 0 && offset >= EPS && offset <= HALF) {
		setRotation(qFloor(m_rotation / FULL) * FULL);
		++steps;
	} else if(steps > 0 && offset >= HALF && offset <= FULL - EPS) {
		setRotation(qCeil(m_rotation / FULL) * FULL);
		--steps;
	}

	if(steps != 0) {
		setRotation((qRound(m_rotation / FULL) + steps) * FULL);
	}
}

bool CanvasControllerBase::activatePendingHudAction()
{
	if(m_hudActionToActivate.isValid()) {
		HudAction action;
		std::swap(action, m_hudActionToActivate);
		if(action.shouldRemoveHoverOnTrigger()) {
			clearHudHover();
		}
		activateHudAction(action, m_hudActionGlobalPos);
		return true;
	} else {
		return false;
	}
}

void CanvasControllerBase::emitTransformChanged()
{
	emit transformChanged(zoom(), rotation());
}

void CanvasControllerBase::emitViewRectChanged()
{
	if(!m_canvasSizeChanging) {
		QPolygonF view = mapRectToCanvasF(viewRectF());
		QRect area =
			canvasRect().intersected(view.boundingRect().toAlignedRect());
		m_canvasViewTileArea = QRect(
			QPoint(area.left() / DP_TILE_SIZE, area.top() / DP_TILE_SIZE),
			QPoint(area.right() / DP_TILE_SIZE, area.bottom() / DP_TILE_SIZE));
		emit viewChanged(view);
		if(m_canvasModel) {
			m_canvasModel->paintEngine()->setCanvasViewTileArea(
				m_canvasViewTileArea);
		}
	}
}

void CanvasControllerBase::emitScrollAreaChanged()
{
	if(!m_canvasSizeChanging) {
		if(m_posBounds.isValid()) {
			QRect page =
				m_invertedTransform.mapRect(viewRectF()).toAlignedRect();
			int minH = m_posBounds.left();
			int maxH = m_posBounds.right();
			int valueH = m_pos.x();
			int pageStepH = qRound(page.width() * m_zoom);
			int singleStepH = qMax(1, pageStepH / 20);
			int minV = m_posBounds.top();
			int maxV = m_posBounds.bottom();
			int valueV = m_pos.y();
			int pageStepV = qRound(page.height() * m_zoom);
			int singleStepV = qMax(1, pageStepV / 20);
			emit scrollAreaChanged(
				minH, maxH, valueH, pageStepH, singleStepH, minV, maxV, valueV,
				pageStepV, singleStepV);
		} else {
			emit scrollAreaChanged(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		}
	}
}

void CanvasControllerBase::emitPenModify(Qt::KeyboardModifiers modifiers)
{
	CanvasShortcuts::ConstraintMatch match =
		m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
	emit penModify(match.toolConstraint1(), match.toolConstraint2());
}

void CanvasControllerBase::translateByViewTransformOffset(
	QTransform &prev, QTransform &cur)
{
	QPointF t = viewTransformOffset();
	qreal tx = t.x();
	qreal ty = t.y();
	if(tx != 0.0 && ty != 0.0) {
		prev.translate(tx, ty);
		cur.translate(tx, ty);
	}
}

QPointF CanvasControllerBase::cursorPosOrCenter() const
{
	return mapPointToCanvasF(viewCursorPosOrCenter());
}

QString CanvasControllerBase::getZoomNoticeText() const
{
	return QCoreApplication::translate("widgets::CanvasView", "Zoom: %1%")
		.arg(zoom() * 100.0, 0, 'f', 2);
}

QString CanvasControllerBase::getRotationNoticeText() const
{
	return QCoreApplication::translate("widgets::CanvasView", "Rotation: %1°")
		.arg(rotation(), 0, 'f', 2);
}

void CanvasControllerBase::showTouchTransformNotice()
{
	TouchHandler *touch = touchHandler();
	if(touch->isTouchPinchEnabled()) {
		if(touch->isTouchTwistEnabled()) {
			showTransformNotice(QStringLiteral("%1\n%2").arg(
				getZoomNoticeText(), getRotationNoticeText()));
		} else {
			showTransformNotice(getZoomNoticeText());
		}
	} else if(touch->isTouchTwistEnabled()) {
		showTransformNotice(getRotationNoticeText());
	}
}

void CanvasControllerBase::showTransformNotice(const QString &text)
{
	bool changed = !m_blockNotices && m_showTransformNotices &&
				   showHudTransformNotice(text);
	if(changed) {
		emit transformNoticeChanged();
	}
}

void CanvasControllerBase::updateLockNotice()
{
	QStringList descriptions = m_lockDescriptions;

	if(m_saveInProgress) {
#ifdef __EMSCRIPTEN__
		descriptions.prepend(
			QCoreApplication::translate("widgets::CanvasView", "Downloading…"));
#else
		descriptions.prepend(
			QCoreApplication::translate("widgets::CanvasView", "Saving…"));
#endif
	}

	QString description = descriptions.join('\n');
	bool changed = description.isEmpty() && m_lockActions.isEmpty()
					   ? hideHudLockStatus()
					   : showHudLockStatus(description, m_lockActions);

	if(changed) {
		emit lockNoticeChanged();
	}
}
}
