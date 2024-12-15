// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "dpmsg/blend_mode.h"
}
#include "desktop/main.h"
#include "desktop/scene/toggleitem.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/touchhandler.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasinterface.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolstate.h"
#include <QDateTime>
#include <QGestureEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QTabletEvent>
#include <QTouchEvent>
#include <QWheelEvent>
#include <cmath>

using libclient::settings::zoomMax;
using libclient::settings::zoomMin;

namespace view {

class CanvasController::SetDragParams {
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

CanvasController::CanvasController(CanvasScene *scene, QWidget *parent)
	: QObject(parent)
	, m_scene(scene)
	, m_dotCursor(generateDotCursor())
	, m_triangleRightCursor(
		  QCursor(QPixmap(":/cursors/triangle-right.png"), 14, 14))
	, m_triangleLeftCursor(
		  QCursor(QPixmap(":/cursors/triangle-left.png"), 14, 14))
	, m_eraserCursor(QCursor(QPixmap(":/cursors/eraser.png"), 2, 2))
	, m_colorPickCursor(QCursor(QPixmap(":/cursors/colorpicker.png"), 2, 29))
	, m_layerPickCursor(QCursor(QPixmap(":/cursors/layerpicker.png"), 2, 29))
	, m_zoomCursor(QCursor(QPixmap(":/cursors/zoom.png"), 8, 8))
	, m_rotateCursor(QCursor(QPixmap(":/cursors/rotate.png"), 16, 16))
	, m_rotateDiscreteCursor(
		  QCursor(QPixmap(":/cursors/rotate-discrete.png"), 16, 16))
	, m_checkCursor(QCursor(QPixmap(":/cursors/check.png"), 16, 16))
	, m_currentCursor(Qt::BlankCursor)
	, m_brushCursorStyle(int(Cursor::TriangleRight))
	, m_eraseCursorStyle(int(Cursor::SameAsBrush))
	, m_alphaLockCursorStyle(int(Cursor::SameAsBrush))
	, m_brushBlendMode(DP_BLEND_MODE_NORMAL)
	, m_touch(new TouchHandler(this))
	, m_toolState(int(tools::ToolState::Normal))
	, m_hudActionToActivate(int(drawingboard::ToggleItem::Action::None))
#ifdef Q_OS_LINUX
	, m_waylandWorkarounds(
		  QGuiApplication::platformName() == QStringLiteral("wayland"))
#endif
{
	DrawpileApp &app = dpApp();
	connect(
		&app, &DrawpileApp::tabletDriverChanged, this,
		&CanvasController::resetTabletFilter, Qt::QueuedConnection);

	desktop::settings::Settings &settings = app.settings();
	settings.bindCanvasViewBackgroundColor(
		this, &CanvasController::setClearColor);
	settings.bindRenderSmooth(this, &CanvasController::setRenderSmooth);
	settings.bindTabletEvents(this, &CanvasController::setTabletEnabled);
	settings.bindIgnoreMouse(this, &CanvasController::setIgnoreMouse);
	settings.bindBrushOutlineWidth(this, &CanvasController::setOutlineWidth);
	settings.bindGlobalPressureCurve(
		this, &CanvasController::setSerializedPressureCurve);
	settings.bindCanvasShortcuts(this, &CanvasController::setCanvasShortcuts);
	settings.bindShowTransformNotices(
		this, &CanvasController::setShowTransformNotices);
	settings.bindBrushCursor(this, &CanvasController::setBrushCursorStyle);
	settings.bindEraseCursor(this, &CanvasController::setEraseCursorStyle);
	settings.bindAlphaLockCursor(
		this, &CanvasController::setAlphaLockCursorStyle);
	settings.bindTabletPressTimerDelay(
		this, &CanvasController::setTabletEventTimerDelay);

	connect(
		m_touch, &TouchHandler::touchPressed, this,
		&CanvasController::touchPressEvent, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchMoved, this,
		&CanvasController::touchMoveEvent, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchReleased, this,
		&CanvasController::touchReleaseEvent, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchScrolledBy, this,
		&CanvasController::scrollByF, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchZoomedRotated, this,
		&CanvasController::touchZoomRotate, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchTapActionActivated, this,
		&CanvasController::touchTapActionActivated, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchColorPicked, this,
		&CanvasController::touchColorPick, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchColorPickFinished, this,
		&CanvasController::hideSceneColorPick, Qt::DirectConnection);

	resetCanvasTransform();
}

void CanvasController::setCanvasWidget(CanvasInterface *canvasWidget)
{
	m_canvasWidget = canvasWidget;
}

void CanvasController::setCanvasModel(canvas::CanvasModel *canvasModel)
{
	m_canvasModel = canvasModel;
	updateCanvasSize(0, 0, 0, 0);
	if(canvasModel) {
		canvas::PaintEngine *pe = canvasModel->paintEngine();
		connect(
			pe, &canvas::PaintEngine::tileCacheDirtyCheckNeeded, this,
			&CanvasController::tileCacheDirtyCheckNeeded,
			pe->isTileCacheDirtyCheckOnTick() ? Qt::DirectConnection
											  : Qt::QueuedConnection);
	}
}

bool CanvasController::shouldRenderSmooth() const
{
	return m_renderSmooth && m_zoom <= 1.99 &&
		   !(std::fabs(m_zoom - 1.0) < 0.01 &&
			 std::fmod(std::fabs(rotation()), 90.0) < 0.01);
}

bool CanvasController::isTouchDrawEnabled() const
{
	return m_touch->isTouchDrawEnabled();
}

bool CanvasController::isTouchPanEnabled() const
{
	return m_touch->isTouchPanEnabled();
}

bool CanvasController::isTouchDrawOrPanEnabled() const
{
	return m_touch->isTouchDrawOrPanEnabled();
}

void CanvasController::setCanvasVisible(bool canvasVisible)
{
	if(m_canvasVisible != canvasVisible) {
		m_canvasVisible = canvasVisible;
		emit canvasVisibleChanged();
	}
}

qreal CanvasController::rotation() const
{
	qreal r = isRotationInverted() ? 360.0 - m_rotation : m_rotation;
	return r > 180.0 ? r - 360.0 : r;
}

void CanvasController::scrollTo(const QPointF &point)
{
	updateCanvasTransform([&] {
		QTransform matrix = calculateCanvasTransformFrom(
			QPointF(), m_zoom, m_rotation, m_mirror, m_flip);
		m_pos = matrix.map(
			point - viewTransformOffset() / (m_zoom > 0.0 ? m_zoom : 1.0));
	});
}

void CanvasController::scrollBy(int x, int y)
{
	scrollByF(x, y);
}

void CanvasController::scrollByF(qreal x, qreal y)
{
	updateCanvasTransform([&] {
		m_pos.setX(m_pos.x() + x);
		m_pos.setY(m_pos.y() + y);
	});
}

void CanvasController::setZoom(qreal zoom)
{
	setZoomAt(zoom, mapPointToCanvasF(viewCenterF()));
}

void CanvasController::setZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(zoomMin, zoom, zoomMax);
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

void CanvasController::resetZoomCenter()
{
	setZoom(1.0);
}

void CanvasController::resetZoomCursor()
{
	setZoomAt(1.0, cursorPosOrCenter());
}

void CanvasController::zoomTo(const QRect &rect, int steps)
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

void CanvasController::zoomToFit()
{
	setZoomToFit(Qt::Horizontal | Qt::Vertical);
}

void CanvasController::zoomInCenter()
{
	zoomSteps(1);
}

void CanvasController::zoomInCursor()
{
	zoomStepsAt(1, cursorPosOrCenter());
}

void CanvasController::zoomOutCenter()
{
	zoomSteps(-1);
}

void CanvasController::zoomOutCursor()
{
	zoomStepsAt(-1, cursorPosOrCenter());
}

void CanvasController::zoomToFitWidth()
{
	setZoomToFit(Qt::Horizontal);
}

void CanvasController::zoomToFitHeight()
{
	setZoomToFit(Qt::Vertical);
}

void CanvasController::zoomSteps(int steps)
{
	zoomStepsAt(steps, mapPointToCanvasF(viewCenterF()));
}

void CanvasController::zoomStepsAt(int steps, const QPointF &point)
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

void CanvasController::setRotation(qreal degrees)
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

void CanvasController::resetRotation()
{
	setRotation(0.0);
}

void CanvasController::rotateStepClockwise()
{
	setRotation(rotation() + ROTATION_STEP);
}

void CanvasController::rotateStepCounterClockwise()
{
	setRotation(rotation() - ROTATION_STEP);
}

void CanvasController::setFlip(bool flip)
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

void CanvasController::setMirror(bool mirror)
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

QPoint CanvasController::viewCenterPoint() const
{
	return mapPointToCanvasF(viewCenterF()).toPoint();
}

bool CanvasController::isPointVisible(const QPointF &point) const
{
	return mapRectToCanvasF(viewRectF()).containsPoint(point, Qt::OddEvenFill);
}

QRectF CanvasController::screenRect() const
{
	return mapRectToCanvasF(viewRectF()).boundingRect();
}

void CanvasController::updateViewSize()
{
	resetCanvasTransform();
	QSizeF view = viewSizeF();
	m_scene->setSceneRect(0.0, 0.0, view.width(), view.height());
	emitScrollAreaChanged();
	emitViewRectChanged();
}

void CanvasController::updateCanvasSize(
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

void CanvasController::withTileCache(
	const std::function<void(canvas::TileCache &)> &fn)
{
	if(m_canvasModel) {
		m_canvasModel->paintEngine()->withTileCache(fn);
	}
}

void CanvasController::handleEnter()
{
	m_showOutline = true;
	m_scene->setCursorOnCanvas(true);
	updateOutline();
	m_tabletFilter.reset();
}

void CanvasController::handleLeave()
{
	m_showOutline = false;
	m_scene->setCursorOnCanvas(false);
	m_hudActionToActivate = int(drawingboard::ToggleItem::Action::None);
	m_hoveringOverHud = false;
	m_scene->removeHover();
	updateOutline();
	resetCursor();
	m_tabletFilter.reset();
}

void CanvasController::handleFocusIn()
{
	clearKeys();
}

void CanvasController::clearKeys()
{
	m_keysDown.clear();
	setDrag(SetDragParams::fromNone()
				.setPenMode(PenMode::Normal)
				.setUpdateOutline()
				.setResetCursor());
}

void CanvasController::handleMouseMove(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	Qt::MouseButtons buttons = event->buttons();
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_move x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(buttons), unsigned(event->modifiers()),
		unsigned(event->source()), int(m_penState), int(touching),
		qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   m_penState != PenState::TabletDown && !touching &&
	   !shouldIgnoreMouse() &&
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

void CanvasController::handleMousePress(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_press x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(touching), qulonglong(event->timestamp()));

	if(((!m_tabletEnabled || !isSynthetic(event))) &&
	   !isSyntheticTouch(event) && !touching && !shouldIgnoreMouse() &&
	   m_tabletEventTimer.hasExpired()) {
		event->accept();
		penPressEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, 1.0, 0.0, 0.0, 0.0,
			event->button(), getMouseModifiers(event),
			int(tools::DeviceType::Mouse), false);
	}
}

void CanvasController::handleMouseRelease(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_release x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(touching), qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   !touching && !shouldIgnoreMouse()) {
		event->accept();
		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, event->button(),
			getMouseModifiers(event));
	}
}

void CanvasController::handleTabletMove(QTabletEvent *event)
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
		bool ignore = m_tabletFilter.shouldIgnore(event);
		DP_EVENT_LOG(
			"tablet_move spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(buttons), unsigned(event->modifiers()),
			int(m_penState), int(m_touch->isTouching()), unsigned(modifiers),
			int(ignore));
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

void CanvasController::handleTabletPress(QTabletEvent *event)
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
		bool ignore = m_tabletFilter.shouldIgnore(event);
		DP_EVENT_LOG(
			"tablet_press spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(buttons), unsigned(event->modifiers()),
			int(m_penState), int(m_touch->isTouching()), unsigned(modifiers),
			int(ignore));
		if(ignore) {
			return;
		}

		Qt::MouseButton button;
		bool eraserOverride;
#ifdef __EMSCRIPTEN__
		// In the browser, we don't get eraser proximity events, instead an
		// eraser pressed down is reported as button flag 0x20 (Qt::TaskButton).
		if(buttons.testFlag(Qt::TaskButton)) {
			button = Qt::LeftButton;
			eraserOverride = m_enableEraserOverride;
		} else {
			button = event->button();
			eraserOverride = false;
		}
#else
		button = event->button();
		eraserOverride = false;
#endif

		penPressEvent(
			QDateTime::currentMSecsSinceEpoch(), posf,
			qBound(0.0, pressure, 1.0), xTilt, yTilt, rotation, button,
			modifiers, int(tools::DeviceType::Tablet), eraserOverride);
	}
}

void CanvasController::handleTabletRelease(QTabletEvent *event)
{
	if(m_tabletEnabled) {
		event->accept();
		startTabletEventTimer();

		QPointF posf = tabletPosF(event);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		bool ignore = m_tabletFilter.shouldIgnore(event);
		DP_EVENT_LOG(
			"tablet_release spontaneous=%d x=%f y=%f buttons=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(event->spontaneous()), posf.x(), posf.y(),
			unsigned(event->buttons()), int(m_penState),
			int(m_touch->isTouching()), unsigned(modifiers), int(ignore));
		if(ignore) {
			return;
		}

		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, event->button(),
			modifiers);
	}
}

void CanvasController::handleTouchBegin(QTouchEvent *event)
{
	event->accept();
	if(m_hudActionToActivate == int(drawingboard::ToggleItem::Action::None)) {
		const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
		int pointsCount = points.size();
		if(pointsCount > 0) {
			int action =
				m_scene->checkHover(compat::touchPos(points.first()).toPoint());
			if(action == int(drawingboard::ToggleItem::Action::None)) {
				m_touch->handleTouchBegin(event);
			} else {
				m_hudActionToActivate = action;
			}
		}
	}
}

void CanvasController::handleTouchUpdate(QTouchEvent *event)
{
	event->accept();
	if(m_hudActionToActivate == int(drawingboard::ToggleItem::Action::None)) {
		m_touch->handleTouchUpdate(
			event, zoom(), rotation(), devicePixelRatioF());
	}
}

void CanvasController::handleTouchEnd(QTouchEvent *event, bool cancel)
{
	event->accept();
	if(!activatePendingToggleAction()) {
		m_touch->handleTouchEnd(event, cancel);
	}
}

void CanvasController::handleGesture(QGestureEvent *event)
{
	m_touch->handleGesture(event, m_zoom, m_rotation);
}

void CanvasController::handleWheel(QWheelEvent *event)
{
	event->accept();
	QPoint angleDelta = event->angleDelta();
	QPointF posf = wheelPosF(event);
	DP_EVENT_LOG(
		"wheel dx=%d dy=%d posX=%f posY=%f buttons=0x%x modifiers=0x%x "
		"penstate=%d touching=%d",
		angleDelta.x(), angleDelta.y(), posf.x(), posf.y(),
		unsigned(event->buttons()), unsigned(event->modifiers()),
		int(m_penState), int(m_touch->isTouching()));

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
		if(m_allowColorPick && m_canvasModel) {
			QPointF p = mapPointToCanvasF(posf);
			m_canvasModel->pickColor(p.x(), p.y(), 0, 0);
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
	case CanvasShortcuts::TOOL_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::Tool), m_allowToolAdjust,
			deltaY);
		break;
	case CanvasShortcuts::COLOR_H_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorH), m_allowColorPick,
			deltaY);
		break;
	case CanvasShortcuts::COLOR_S_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorS), m_allowColorPick,
			deltaY);
		break;
	case CanvasShortcuts::COLOR_V_ADJUST:
		wheelAdjust(
			event, int(tools::QuickAdjustType::ColorV), m_allowColorPick,
			deltaY);
		break;
	default:
		qWarning("Unhandled mouse wheel canvas shortcut %u", match.action());
		break;
	}
}

void CanvasController::wheelAdjust(
	QWheelEvent *event, int param, bool allowed, int delta)
{
	event->accept();
	if(allowed) {
		emit quickAdjust(param, qreal(delta) / 120.0);
	}
}

void CanvasController::handleKeyPress(QKeyEvent *event)
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
		case CanvasShortcuts::TOOL_ADJUST:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(SetDragParams::fromMouseMatch(mouseMatch)
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
		case CanvasShortcuts::TOOL_ADJUST:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(SetDragParams::fromKeyMatch(keyMatch)
						.setPenMode(PenMode::Normal)
						.setDragMode(ViewDragMode::Started)
						.setResetDragPoints()
						.setResetDragRotation());
			break;
		default:
			CanvasShortcuts::Match mouseMatch =
				m_canvasShortcuts.matchMouseButton(
					modifiers, m_keysDown, Qt::LeftButton);
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowToolAdjust));
				break;
			case CanvasShortcuts::COLOR_H_ADJUST:
			case CanvasShortcuts::COLOR_S_ADJUST:
			case CanvasShortcuts::COLOR_V_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowColorPick));
				break;
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			case CanvasShortcuts::CANVAS_ZOOM:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared));
				break;
			case CanvasShortcuts::COLOR_PICK:
				m_penMode =
					m_allowColorPick ? PenMode::Colorpick : PenMode::Normal;
				break;
			case CanvasShortcuts::LAYER_PICK:
				m_penMode = PenMode::Layerpick;
				break;
			default:
				m_penMode = PenMode::Normal;
				break;
			}
			break;
		}
	} else {
		CanvasShortcuts::ConstraintMatch match =
			m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
		emit penModify(match.toolConstraint1(), match.toolConstraint2());
	}

	updateOutline();
	resetCursor();
}

void CanvasController::handleKeyRelease(QKeyEvent *event)
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
		case CanvasShortcuts::TOOL_ADJUST:
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
		case CanvasShortcuts::TOOL_ADJUST:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(SetDragParams::fromMouseMatch(mouseMatch)
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
		case CanvasShortcuts::TOOL_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(m_allowToolAdjust));
			break;
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(m_allowColorPick));
			break;
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
			setDrag(SetDragParams::fromMouseMatch(mouseMatch)
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
			m_penMode = m_allowColorPick ? PenMode::Colorpick : PenMode::Normal;
			break;
		case CanvasShortcuts::LAYER_PICK:
			m_penMode = PenMode::Layerpick;
			break;
		default:
			m_penMode = PenMode::Normal;
			break;
		}
	} else {
		CanvasShortcuts::ConstraintMatch match =
			m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
		emit penModify(match.toolConstraint1(), match.toolConstraint2());
	}

	updateOutline();
	resetCursor();
}

void CanvasController::setClearColor(const QColor clearColor)
{
	if(clearColor != m_clearColor) {
		m_clearColor = clearColor;
		emit clearColorChanged();
	}
}

void CanvasController::setRenderSmooth(bool renderSmooth)
{
	if(renderSmooth != m_renderSmooth) {
		m_renderSmooth = renderSmooth;
		emit renderSmoothChanged();
	}
}

void CanvasController::setTabletEnabled(bool tabletEnabled)
{
	m_tabletEnabled = tabletEnabled;
}

void CanvasController::setIgnoreMouse(bool ignoreMouse)
{
	m_ignoreMouse = ignoreMouse;
}

void CanvasController::setSerializedPressureCurve(
	const QString &serializedPressureCurve)
{
	KisCubicCurve pressureCurve;
	pressureCurve.fromString(serializedPressureCurve);
	m_pressureCurve = pressureCurve;
}

void CanvasController::setOutlineWidth(qreal outlineWidth)
{
	if(outlineWidth != m_outlineWidth) {
		changeOutline([&]() {
			m_outlineWidth = outlineWidth;
		});
	}
}

void CanvasController::setCanvasShortcuts(QVariantMap canvasShortcuts)
{
	m_canvasShortcuts = CanvasShortcuts::load(canvasShortcuts);
}

void CanvasController::updatePixelGridScale()
{
	qreal pixelGridScale = m_pixelGrid && m_zoom >= 8.0 ? m_zoom : 0.0;
	if(pixelGridScale != m_pixelGridScale) {
		m_pixelGridScale = pixelGridScale;
		emit pixelGridScaleChanged();
	}
}

void CanvasController::setLockReasons(QFlags<view::Lock::Reason> reasons)
{
	bool locked = reasons;
	if(locked != m_locked) {
		m_locked = reasons;
		updateOutline();
		resetCursor();
	}
}

void CanvasController::setToolState(int toolState)
{
	if(toolState != m_toolState) {
		m_toolState = toolState;
		updateOutline();
		resetCursor();
	}
}

void CanvasController::setLockDescription(const QString &lockDescription)
{
	m_lockDescription = lockDescription;
	updateLockNotice();
}

void CanvasController::setSaveInProgress(bool saveInProgress)
{
	if(saveInProgress != m_saveInProgress) {
		m_saveInProgress = saveInProgress;
		emit saveInProgressChanged(saveInProgress);
		updateLockNotice();
	}
}

void CanvasController::setOutlineSize(int outlineSize)
{
	if(outlineSize != m_outlineSize) {
		changeOutline([&]() {
			m_outlineSize = outlineSize;
		});
	}
}

void CanvasController::setOutlineMode(bool subpixel, bool square, bool force)
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

void CanvasController::setPixelGrid(bool pixelGrid)
{
	if(pixelGrid != m_pixelGrid) {
		m_pixelGrid = pixelGrid;
		updatePixelGridScale();
	}
}

void CanvasController::setPointerTracking(bool pointerTracking)
{
	m_pointerTracking = pointerTracking;
}

void CanvasController::setToolCapabilities(
	bool allowColorPick, bool allowToolAdjust, bool toolHandlesRightClick,
	bool fractionalTool, bool toolSupportsPressure)
{
	m_allowColorPick = allowColorPick;
	m_allowToolAdjust = allowToolAdjust;
	m_toolHandlesRightClick = toolHandlesRightClick;
	m_fractionalTool = fractionalTool;
	m_toolSupportsPressure = toolSupportsPressure;
	m_touch->setAllowColorPick(allowColorPick);
}

void CanvasController::setToolCursor(const QCursor &toolCursor)
{
	if(toolCursor != m_toolCursor) {
		m_toolCursor = toolCursor;
		resetCursor();
	}
}

void CanvasController::setBrushBlendMode(int brushBlendMode)
{
	if(brushBlendMode != m_brushBlendMode) {
		m_brushBlendMode = brushBlendMode;
		resetCursor();
	}
}

#ifdef __EMSCRIPTEN__
void CanvasController::setEnableEraserOverride(bool enableEraserOverride)
{
	m_enableEraserOverride = enableEraserOverride;
}
#endif

bool CanvasController::isOutlineVisible() const
{
	return m_showOutline && m_outlineVisibleInMode && m_outlineSize > 0.0 &&
		   m_outlineWidth > 0.0;
}

QPointF CanvasController::outlinePos() const
{
	if(m_subpixelOutline) {
		return m_outlinePos;
	} else {
		QPointF pixelPos(
			std::floor(m_outlinePos.x()), std::floor(m_outlinePos.y()));
		if(m_outlineSize % 2 == 0) {
			return pixelPos;
		} else {
			return pixelPos + QPointF(0.5, 0.5);
		}
	}
}

void CanvasController::setShowTransformNotices(bool showTransformNotices)
{
	m_showTransformNotices = showTransformNotices;
}

void CanvasController::setTabletEventTimerDelay(int tabletEventTimerDelay)
{
	m_tabletEventTimerDelay = tabletEventTimerDelay;
	m_tabletEventTimer.setRemainingTime(0);
}

void CanvasController::startTabletEventTimer()
{
	m_touch->onTabletEventReceived();
	if(m_tabletEventTimerDelay > 0) {
		m_tabletEventTimer.setRemainingTime(m_tabletEventTimerDelay);
	}
}

void CanvasController::resetTabletFilter()
{
	m_tabletFilter.reset();
}

void CanvasController::penMoveEvent(
	long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::KeyboardModifiers modifiers)
{
	if(m_hudActionToActivate != int(drawingboard::ToggleItem::Action::None)) {
		return;
	}

	canvas::Point point =
		mapPenPointToCanvasF(timeMsec, posf, pressure, xtilt, ytilt, rotation);
	emit coordinatesChanged(point);

	m_scene->setCursorPos(posf);

	if(m_dragMode == ViewDragMode::Started) {
		moveDrag(posf.toPoint());
	} else {
		if(m_fractionalTool || !m_prevPoint.intSame(point)) {
			if(m_penState == PenState::Up) {
				emit penHover(
					point, m_rotation, m_zoom / devicePixelRatioF(), m_mirror,
					m_flip);
				if(m_pointerTracking && m_canvasModel) {
					emit pointerMove(point);
				}

				bool wasHovering;
				m_hoveringOverHud =
					m_scene->checkHover(posf.toPoint(), &wasHovering) !=
					int(drawingboard::ToggleItem::Action::None);
				if(m_hoveringOverHud != wasHovering) {
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
						if(!m_locked) {
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

void CanvasController::penPressEvent(
	long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers, int deviceType, bool eraserOverride)
{
	if(m_hudActionToActivate != int(drawingboard::ToggleItem::Action::None)) {
		return;
	}

	if(m_penState == PenState::Up) {
		bool wasHovering;
		int action = m_scene->checkHover(posf.toPoint(), &wasHovering);
		m_hoveringOverHud =
			action != int(drawingboard::ToggleItem::Action::None);
		if(m_hoveringOverHud != wasHovering) {
			updateOutline();
			resetCursor();
		}
		if(m_hoveringOverHud) {
			m_hudActionToActivate = action;
			return;
		}

		CanvasShortcuts::Match match =
			m_canvasShortcuts.matchMouseButton(modifiers, m_keysDown, button);
		// If the tool wants to receive right clicks (e.g. to undo the last
		// point in a bezier curve), we have to override that shortcut.
		if(m_toolHandlesRightClick &&
		   match.isUnmodifiedClick(Qt::RightButton)) {
			match.shortcut = nullptr;
		}

		PenMode penMode = PenMode::Normal;

		switch(match.action()) {
		case CanvasShortcuts::NO_ACTION:
			break;
		case CanvasShortcuts::TOOL_ADJUST:
			setDrag(SetDragParams::fromMouseMatch(match)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(
							m_allowToolAdjust &&
							m_dragMode != ViewDragMode::Started)
						.setUpdateOutline());
			break;
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(SetDragParams::fromMouseMatch(match)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(
							m_allowColorPick &&
							m_dragMode != ViewDragMode::Started));
			break;
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
			setDrag(SetDragParams::fromMouseMatch(match)
						.setDragMode(ViewDragMode::Prepared)
						.setDragCondition(m_dragMode != ViewDragMode::Started));
			break;
		case CanvasShortcuts::COLOR_PICK:
			if(m_allowColorPick) {
				penMode = PenMode::Colorpick;
			}
			break;
		case CanvasShortcuts::LAYER_PICK:
			penMode = PenMode::Layerpick;
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
					if(!m_locked) {
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

void CanvasController::penReleaseEvent(
	long long timeMsec, const QPointF &posf, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers)
{
	activatePendingToggleAction();

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
		case CanvasShortcuts::TOOL_ADJUST:
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(SetDragParams::fromMouseMatch(mouseMatch)
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

		if(!m_locked && m_penMode == PenMode::Normal) {
			CanvasShortcuts::ConstraintMatch constraintMatch =
				m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
			emit penUp(
				constraintMatch.toolConstraint1(),
				constraintMatch.toolConstraint2());
		}

		if(m_pickingColor) {
			hideSceneColorPick();
		}

		m_penState = PenState::Up;

		m_hoveringOverHud = m_scene->checkHover(posf.toPoint()) !=
							int(drawingboard::ToggleItem::Action::None);
		if(!m_hoveringOverHud) {
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowToolAdjust));
				break;
			case CanvasShortcuts::COLOR_H_ADJUST:
			case CanvasShortcuts::COLOR_S_ADJUST:
			case CanvasShortcuts::COLOR_V_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowColorPick));
				break;
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			case CanvasShortcuts::CANVAS_ZOOM:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared));
				break;
			case CanvasShortcuts::COLOR_PICK:
				if(m_allowColorPick) {
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

void CanvasController::touchPressEvent(
	QEvent *event, long long timeMsec, const QPointF &posf)
{
	Q_UNUSED(event);
	penPressEvent(
		timeMsec, posf, 1.0, 0.0, 0.0, 0.0, Qt::LeftButton, Qt::NoModifier,
		int(tools::DeviceType::Touch), false);
}

void CanvasController::touchMoveEvent(long long timeMsec, const QPointF &posf)
{
	penMoveEvent(timeMsec, posf, 1.0, 0.0, 0.0, 0.0, Qt::NoModifier);
}

void CanvasController::touchReleaseEvent(
	long long timeMsec, const QPointF &posf)
{
	penReleaseEvent(timeMsec, posf, Qt::LeftButton, Qt::NoModifier);
}

void CanvasController::touchZoomRotate(qreal zoom, qreal rotation)
{
	{
		QScopedValueRollback<bool> rollback(m_blockNotices, true);
		setZoom(zoom);
		setRotation(rotation);
	}
	showTouchTransformNotice();
}

void CanvasController::setDrag(const SetDragParams &params)
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
			m_scene->showColorPick(
				int(tools::ColorPickSource::Adjust), m_dragLastPoint);
		}
	} else if(
		prevMode == ViewDragMode::Started &&
		CanvasShortcuts::isColorAdjustAction(prevAction)) {
		m_scene->hideColorPick();
	}
}

void CanvasController::moveDrag(const QPoint &point)
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
	case CanvasShortcuts::TOOL_ADJUST:
		dragAdjust(int(tools::QuickAdjustType::Tool), deltaX, 1.2);
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

void CanvasController::dragAdjust(int type, int delta, qreal acceleration)
{
	// Horizontally, dragging right (+X) is higher and left (-X) is lower,
	// but vertically, dragging up (-Y) is higher and down (+Y) is lower.
	// We have to invert in one of those cases to match with that logic.
	qreal d = qreal(m_dragSwapAxes ? delta : -delta);
	if(acceleration != -1) {
		d = std::copysign(std::pow(std::abs(d), acceleration), d);
	}
	emit quickAdjust(type, d * 0.1);
}

void CanvasController::pickColor(
	int source, const QPointF &point, const QPointF &posf)
{
	m_canvasModel->pickColor(point.x(), point.y(), 0, 0);
	m_pickingColor = m_scene->showColorPick(source, posf);
}

void CanvasController::touchColorPick(const QPointF &posf)
{
	pickColor(
		int(tools::ColorPickSource::Touch), mapPointToCanvasF(posf), posf);
}

void CanvasController::hideSceneColorPick()
{
	m_scene->hideColorPick();
	m_pickingColor = false;
}

void CanvasController::resetCursor()
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
			setViewportCursor(m_rotateCursor);
			break;
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			setViewportCursor(m_rotateDiscreteCursor);
			break;
		case CanvasShortcuts::CANVAS_ZOOM:
			setViewportCursor(m_zoomCursor);
			break;
		case CanvasShortcuts::TOOL_ADJUST:
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
		setViewportCursor(m_colorPickCursor);
		return;
	case PenMode::Layerpick:
		setViewportCursor(m_layerPickCursor);
		return;
	}

	if(m_locked) {
		setViewportCursor(Qt::ForbiddenCursor);
		return;
	} else if(m_toolState == int(tools::ToolState::Busy)) {
		setViewportCursor(Qt::WaitCursor);
		return;
	}

	if(m_toolCursor.shape() == Qt::CrossCursor) {
		switch(getCurrentCursorStyle()) {
		case int(view::Cursor::Dot):
			setViewportCursor(m_dotCursor);
			break;
		case int(view::Cursor::Cross):
			setViewportCursor(Qt::CrossCursor);
			break;
		case int(view::Cursor::Arrow):
			setViewportCursor(Qt::ArrowCursor);
			break;
		case int(view::Cursor::TriangleLeft):
			setViewportCursor(m_triangleLeftCursor);
			break;
		case int(view::Cursor::TriangleRight):
			setViewportCursor(m_triangleRightCursor);
			break;
		case int(view::Cursor::Eraser):
			setViewportCursor(m_eraserCursor);
			break;
		default:
			setViewportCursor(m_triangleRightCursor);
			break;
		}
	} else {
		setViewportCursor(m_toolCursor);
	}
}

int CanvasController::getCurrentCursorStyle() const
{
	if(DP_blend_mode_presents_as_eraser(m_brushBlendMode)) {
		if(m_eraseCursorStyle != int(view::Cursor::SameAsBrush)) {
			return m_eraseCursorStyle;
		}
	} else if(DP_blend_mode_preserves_alpha(m_brushBlendMode)) {
		if(m_alphaLockCursorStyle != int(view::Cursor::SameAsBrush)) {
			return m_alphaLockCursorStyle;
		}
	}
	return m_brushCursorStyle;
}

void CanvasController::setBrushCursorStyle(int brushCursorStyle)
{
	if(brushCursorStyle != m_brushCursorStyle) {
		m_brushCursorStyle = brushCursorStyle;
		resetCursor();
	}
}

void CanvasController::setEraseCursorStyle(int eraseCursorStyle)
{
	if(eraseCursorStyle != m_eraseCursorStyle) {
		m_eraseCursorStyle = eraseCursorStyle;
		resetCursor();
	}
}

void CanvasController::setAlphaLockCursorStyle(int alphaLockCursorStyle)
{
	if(alphaLockCursorStyle != m_alphaLockCursorStyle) {
		m_alphaLockCursorStyle = alphaLockCursorStyle;
		resetCursor();
	}
}

void CanvasController::updateOutlinePos(QPointF point)
{
	if(!canvas::Point::roughlySame(point, m_outlinePos)) {
		setOutlinePos(point);
		if(isOutlineVisible()) {
			emit outlineChanged();
		}
	}
}

void CanvasController::setOutlinePos(QPointF point)
{
	m_outlinePos = point;
}

void CanvasController::updateOutline()
{
	updateOutlineVisibleInMode();
	emit outlineChanged();
}

void CanvasController::updateOutlineVisibleInMode()
{
	m_outlineVisibleInMode =
		!m_hoveringOverHud &&
		(m_dragMode == ViewDragMode::None
			 ? m_penMode == PenMode::Normal && !m_locked &&
				   m_toolState == int(tools::ToolState::Normal)
			 : m_dragAction == CanvasShortcuts::TOOL_ADJUST);
}

QPointF CanvasController::getOutlinePos() const
{
	QPointF pos = mapPointFromCanvasF(m_outlinePos);
	if(!m_subpixelOutline && m_outlineSize % 2 == 0) {
		qreal offset = actualZoom() * 0.5;
		pos -= QPointF(offset, offset);
	}
	return pos;
}

qreal CanvasController::getOutlineWidth() const
{
	return m_forceOutline ? qMax(1.0, m_outlineWidth) : m_outlineWidth;
}

void CanvasController::changeOutline(const std::function<void()> &block)
{
	bool wasVisible = isOutlineVisible();
	block();
	bool isVisible = isOutlineVisible();
	if(isVisible || wasVisible != isVisible) {
		emit outlineChanged();
	}
}

QCursor CanvasController::generateDotCursor()
{
	QPixmap dot(8, 8);
	dot.fill(Qt::transparent);
	QPainter p(&dot);
	p.setPen(Qt::white);
	p.drawPoint(0, 0);
	return QCursor(dot, 0, 0);
}

void CanvasController::setViewportCursor(const QCursor &cursor)
{
	if(m_canvasWidget) {
#ifdef HAVE_EMULATED_BITMAP_CURSOR
		m_scene->setCursor(cursor);
		QCursor actualCursor =
			cursor.shape() == Qt::BitmapCursor ? Qt::BlankCursor : cursor;
		if(actualCursor != m_currentCursor) {
			m_currentCursor = actualCursor;
			emit cursorChanged(actualCursor);
		}
#else
		if(cursor != m_currentCursor) {
			m_currentCursor = cursor;
			emit cursorChanged(cursor);
		}
#endif
	}
}

QPointF CanvasController::mousePosF(QMouseEvent *event) const
{
	return compat::mousePosition(*event);
}

QPointF CanvasController::tabletPosF(QTabletEvent *event) const
{
	return compat::tabPosF(*event);
}

QPointF CanvasController::wheelPosF(QWheelEvent *event) const
{
	return compat::wheelPosition(*event);
}

bool CanvasController::isSynthetic(QMouseEvent *event)
{
	return event->source() & Qt::MouseEventSynthesizedByQt;
}

bool CanvasController::isSyntheticTouch(QMouseEvent *event)
{
#ifdef Q_OS_WIN
	// Windows may generate bogus mouse events from touches, we never want this.
	return event->source() & Qt::MouseEventSynthesizedBySystem;
#else
	Q_UNUSED(event);
	return false;
#endif
}

Qt::KeyboardModifiers
CanvasController::getKeyboardModifiers(const QKeyEvent *event) const
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
CanvasController::getMouseModifiers(const QMouseEvent *event) const
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
CanvasController::getTabletModifiers(const QTabletEvent *event) const
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
CanvasController::getWheelModifiers(const QWheelEvent *event) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(event);
	return getFallbackModifiers();
#else
	return event->modifiers();
#endif
}

Qt::KeyboardModifiers CanvasController::getFallbackModifiers() const
{
	Qt::KeyboardModifiers mods;
	mods.setFlag(Qt::ControlModifier, m_keysDown.contains(Qt::Key_Control));
	mods.setFlag(Qt::ShiftModifier, m_keysDown.contains(Qt::Key_Shift));
	mods.setFlag(Qt::AltModifier, m_keysDown.contains(Qt::Key_Alt));
	mods.setFlag(Qt::MetaModifier, m_keysDown.contains(Qt::Key_Meta));
	return mods;
}

canvas::Point CanvasController::mapPenPointToCanvasF(
	long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation) const
{
	return canvas::Point(
		timeMsec, mapPointToCanvasF(point), m_pressureCurve.value(pressure),
		xtilt, ytilt, rotation);
}

QPointF CanvasController::mapPointFromCanvasF(const QPointF &point) const
{
	return m_transform.map(point);
}

QPolygonF CanvasController::mapRectFromCanvas(const QRect &rect) const
{
	return m_transform.map(QRectF(rect));
}

QPointF CanvasController::mapPointToCanvas(const QPoint &point) const
{
	return mapPointToCanvasF(QPointF(point));
}

QPointF CanvasController::mapPointToCanvasF(const QPointF &point) const
{
	return m_invertedTransform.map(point + viewToCanvasOffset());
}

QPolygonF CanvasController::mapRectToCanvasF(const QRectF &rect) const
{
	return m_invertedTransform.map(rect.translated(viewToCanvasOffset()));
}

QPoint CanvasController::mapPointFromGlobal(const QPoint &point) const
{
	return m_canvasWidget ? m_canvasWidget->asWidget()->mapFromGlobal(point)
						  : point;
}

void CanvasController::updateCanvasTransform(const std::function<void()> &block)
{
	block();
	updatePosBounds();
	resetCanvasTransform();
}

void CanvasController::resetCanvasTransform()
{
	m_transform = calculateCanvasTransform();
	m_invertedTransform = m_transform.inverted();
	updateSceneTransform();
	emitScrollAreaChanged();
	emitTransformChanged();
	emitViewRectChanged();
	updatePixelGridScale();
}

void CanvasController::updateSceneTransform()
{
	m_scene->setCanvasTransform(calculateCanvasTransformFrom(
		m_pos + viewToCanvasOffset(), m_zoom, m_rotation, m_mirror, m_flip));
	m_scene->setZoom(m_zoom / devicePixelRatioF());
}

void CanvasController::updatePosBounds()
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

void CanvasController::clampPos()
{
	if(m_posBounds.isValid()) {
		m_pos.setX(qBound(m_posBounds.left(), m_pos.x(), m_posBounds.right()));
		m_pos.setY(qBound(m_posBounds.top(), m_pos.y(), m_posBounds.bottom()));
	}
}

QTransform CanvasController::calculateCanvasTransformFrom(
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

void CanvasController::mirrorFlip(QTransform &matrix, bool mirror, bool flip)
{
	matrix.scale(mirror ? -1.0 : 1.0, flip ? -1.0 : 1.0);
}

void CanvasController::setZoomToFit(Qt::Orientations orientations)
{
	if(m_canvasWidget) {
		qreal dpr = devicePixelRatioF();
		QRectF r = QRectF(QPointF(0.0, 0.0), QSizeF(m_canvasSize) / dpr);
		QSize widgetSize = m_canvasWidget->viewSize();
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
			qreal newZoom = qBound(zoomMin, scale, zoomMax);
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

void CanvasController::setRotationSnap(qreal degrees)
{
	setRotation(qAbs(std::fmod(degrees, 360.0)) < 5.0 ? 0.0 : degrees);
}

void CanvasController::rotateByDiscreteSteps(int steps)
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

bool CanvasController::activatePendingToggleAction()
{
	if(int action = m_hudActionToActivate;
	   action != int(drawingboard::ToggleItem::Action::None)) {
		m_hudActionToActivate = int(drawingboard::ToggleItem::Action::None);
		m_hoveringOverHud = false;
		m_scene->removeHover();
		resetCursor();
		emit toggleActionActivated(action);
		return true;
	} else {
		return false;
	}
}

void CanvasController::emitTransformChanged()
{
	emit transformChanged(zoom(), rotation());
}

void CanvasController::emitViewRectChanged()
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

void CanvasController::emitScrollAreaChanged()
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

QRect CanvasController::canvasRect() const
{
	return QRect(QPoint(0, 0), m_canvasSize);
}

QRectF CanvasController::canvasRectF() const
{
	return QRectF(canvasRect());
}

QPointF CanvasController::viewCenterF() const
{
	return viewRectF().center();
}

QRectF CanvasController::viewRectF() const
{
	return QRectF(QPointF(0.0, 0.0), viewSizeF());
}

QSize CanvasController::viewSize() const
{
	return m_canvasWidget ? m_canvasWidget->viewSize() : QSize();
}

QSizeF CanvasController::viewSizeF() const
{
	return QSizeF(viewSize());
}

qreal CanvasController::devicePixelRatioF() const
{
	return m_canvasWidget ? m_canvasWidget->asWidget()->devicePixelRatioF()
						  : dpApp().devicePixelRatio();
}

QPointF CanvasController::viewToCanvasOffset() const
{
	return m_canvasWidget ? m_canvasWidget->viewToCanvasOffset()
						  : QPointF(0.0, 0.0);
}

QPointF CanvasController::viewTransformOffset() const
{
	return m_canvasWidget ? m_canvasWidget->viewTransformOffset()
						  : QPointF(0.0, 0.0);
}

void CanvasController::translateByViewTransformOffset(
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

QPointF CanvasController::cursorPosOrCenter() const
{
	return mapPointToCanvasF(
		m_scene->isCursorOnCanvas() ? m_scene->cursorPos() : viewCenterF());
}

QString CanvasController::getZoomNoticeText() const
{
	return QCoreApplication::translate("widgets::CanvasView", "Zoom: %1%")
		.arg(zoom() * 100.0, 0, 'f', 2);
}

QString CanvasController::getRotationNoticeText() const
{
	return QCoreApplication::translate("widgets::CanvasView", "Rotation: %1°")
		.arg(rotation(), 0, 'f', 2);
}

void CanvasController::showTouchTransformNotice()
{
	if(m_touch->isTouchPinchEnabled()) {
		if(m_touch->isTouchTwistEnabled()) {
			showTransformNotice(QStringLiteral("%1\n%2").arg(
				getZoomNoticeText(), getRotationNoticeText()));
		} else {
			showTransformNotice(getZoomNoticeText());
		}
	} else if(m_touch->isTouchTwistEnabled()) {
		showTransformNotice(getRotationNoticeText());
	}
}

void CanvasController::showTransformNotice(const QString &text)
{
	bool changed = !m_blockNotices && m_showTransformNotices &&
				   m_scene->showTransformNotice(text);
	if(changed) {
		emit transformNoticeChanged();
	}
}

void CanvasController::updateLockNotice()
{
	QStringList descriptions;

	if(m_saveInProgress) {
#ifdef __EMSCRIPTEN__
		descriptions.append(
			QCoreApplication::translate("widgets::CanvasView", "Downloading…"));
#else
		descriptions.append(
			QCoreApplication::translate("widgets::CanvasView", "Saving…"));
#endif
	}

	if(!m_lockDescription.isEmpty()) {
		descriptions.append(m_lockDescription);
	}

	QString description = descriptions.join('\n');
	bool changed = description.isEmpty() ? m_scene->hideLockNotice()
										 : m_scene->showLockNotice(description);

	if(changed) {
		emit lockNoticeChanged();
	}
}
}
