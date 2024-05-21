// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include "dpmsg/blend_mode.h"
}
#include "desktop/main.h"
#include "desktop/scene/toggleitem.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/view/canvascontroller.h"
#include "desktop/view/canvasinterface.h"
#include "desktop/view/canvasscene.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/tools/devicetype.h"
#include "libshared/util/qtcompat.h"
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
	, m_currentCursor(Qt::BlankCursor)
	, m_brushCursorStyle(int(Cursor::TriangleRight))
	, m_eraseCursorStyle(int(Cursor::SameAsBrush))
	, m_alphaLockCursorStyle(int(Cursor::SameAsBrush))
	, m_brushBlendMode(DP_BLEND_MODE_NORMAL)
{
	desktop::settings::Settings &settings = dpApp().settings();
	settings.bindCanvasViewBackgroundColor(
		this, &CanvasController::setClearColor);
	settings.bindRenderSmooth(this, &CanvasController::setRenderSmooth);
	settings.bindTabletEvents(this, &CanvasController::setTabletEnabled);
	settings.bindOneFingerScroll(this, &CanvasController::setEnableTouchScroll);
	settings.bindOneFingerDraw(this, &CanvasController::setEnableTouchDraw);
	settings.bindTwoFingerZoom(this, &CanvasController::setEnableTouchPinch);
	settings.bindTwoFingerRotate(this, &CanvasController::setEnableTouchTwist);
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
		m_pos = matrix.map(point);
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

void CanvasController::resetZoom()
{
	setZoom(1.0);
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

void CanvasController::zoomIn()
{
	zoomSteps(1);
}

void CanvasController::zoomOut()
{
	zoomSteps(-1);
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
}

void CanvasController::handleLeave()
{
	m_showOutline = false;
	m_scene->setCursorOnCanvas(false);
	m_hoveringOverHud = false;
	m_scene->removeHover();
	updateOutline();
	resetCursor();
}

void CanvasController::handleFocusIn()
{
	clearKeys();
}

void CanvasController::clearKeys()
{
	m_keysDown.clear();
	m_dragMode = ViewDragMode::None;
	m_penMode = PenMode::Normal;
	updateOutline();
	resetCursor();
}

void CanvasController::handleMouseMove(QMouseEvent *event)
{
	QPointF posf = mousePosF(event);
	Qt::MouseButtons buttons = event->buttons();
	DP_EVENT_LOG(
		"mouse_move x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(buttons), unsigned(event->modifiers()),
		unsigned(event->source()), int(m_penState), int(m_touching),
		qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   m_penState != PenState::TabletDown && !m_touching) {
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
	DP_EVENT_LOG(
		"mouse_press x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(m_touching), qulonglong(event->timestamp()));

	if(((!m_tabletEnabled || !isSynthetic(event))) &&
	   !isSyntheticTouch(event) && !m_touching) {
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
	DP_EVENT_LOG(
		"mouse_release x=%f y=%f buttons=0x%x modifiers=0x%x source=0x%x "
		"penstate=%d touching=%d timestamp=%llu",
		posf.x(), posf.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()),
		int(m_penState), int(m_touching), qulonglong(event->timestamp()));

	if((!m_tabletEnabled || !isSynthetic(event)) && !isSyntheticTouch(event) &&
	   !m_touching) {
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
		DP_EVENT_LOG(
			"tablet_move spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(event->buttons()),
			unsigned(event->modifiers()), int(m_penState), int(m_touching),
			unsigned(modifiers));

		// Under Windows Ink, some tablets report bogus zero-pressure inputs.
		// We accept them so that they don't result in synthesized mouse events,
		// but don't actually act on them.
		if(m_penState == PenState::Up || pressure != 0.0) {
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

		QPointF posf = tabletPosF(event);
		qreal rotation = qDegreesToRadians(event->rotation());
		Qt::MouseButtons buttons = event->buttons();
		qreal pressure = event->pressure();
		qreal xTilt = event->xTilt();
		qreal yTilt = event->yTilt();
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		DP_EVENT_LOG(
			"tablet_press spontaneous=%d x=%f y=%f pressure=%f xtilt=%f "
			"ytilt=%f rotation=%f buttons=0x%x modifiers=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u",
			int(event->spontaneous()), posf.x(), posf.y(), pressure, xTilt,
			yTilt, rotation, unsigned(buttons), unsigned(event->modifiers()),
			int(m_penState), int(m_touching), unsigned(modifiers));

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

		QPointF posf = tabletPosF(event);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(event);
		DP_EVENT_LOG(
			"tablet_release spontaneous=%d x=%f y=%f buttons=0x%x penstate=%d "
			"touching=%d effectivemodifiers=0x%u",
			int(event->spontaneous()), posf.x(), posf.y(),
			unsigned(event->buttons()), int(m_penState), int(m_touching),
			unsigned(modifiers));

		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), posf, event->button(),
			modifiers);
	}
}

void CanvasController::handleTouchBegin(QTouchEvent *event)
{
	event->accept();
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	int pointsCount = points.size();

	QPointF posf = compat::touchPos(points.first());
	int action = m_scene->checkHover(posf.toPoint());
	if(action != int(drawingboard::ToggleItem::Action::None)) {
		emit toggleActionActivated(action);
		m_hoveringOverHud = false;
		m_scene->removeHover();
		resetCursor();
		return;
	}

	m_touchDrawBuffer.clear();
	m_touchRotating = false;
	if(m_enableTouchDraw && pointsCount == 1 && !compat::isTouchPad(event)) {
		DP_EVENT_LOG(
			"touch_draw_begin x=%f y=%f penstate=%d touching=%d type=%d "
			"device=%s points=%s timestamp=%llu",
			posf.x(), posf.y(), int(m_penState), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		if(m_enableTouchScroll || m_enableTouchPinch || m_enableTouchTwist) {
			// Buffer the touch first, since it might end up being the
			// beginning of an action that involves multiple fingers.
			m_touchDrawBuffer.append(
				{QDateTime::currentMSecsSinceEpoch(), posf});
			m_touchMode = TouchMode::Unknown;
		} else {
			// There's no other actions other than drawing enabled, so we
			// can just start drawing without awaiting what happens next.
			m_touchMode = TouchMode::Drawing;
			touchPressEvent(QDateTime::currentMSecsSinceEpoch(), posf);
		}
	} else {
		DP_EVENT_LOG(
			"touch_begin penstate=%d touching=%d type=%d device=%s "
			"points=%s timestamp=%llu",
			int(m_penState), int(m_touching), compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		m_touchMode = TouchMode::Moving;
	}
}

void CanvasController::handleTouchUpdate(QTouchEvent *event)
{
	event->accept();
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	int pointsCount = points.size();

	if(m_enableTouchDraw &&
	   ((pointsCount == 1 && m_touchMode == TouchMode::Unknown) ||
		m_touchMode == TouchMode::Drawing) &&
	   !compat::isTouchPad(event)) {
		QPointF posf = compat::touchPos(compat::touchPoints(*event).first());
		DP_EVENT_LOG(
			"touch_draw_update x=%f y=%f penstate=%d touching=%d type=%d "
			"device=%s points=%s timestamp=%llu",
			posf.x(), posf.y(), int(m_penState), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		int bufferCount = m_touchDrawBuffer.size();
		if(bufferCount == 0) {
			if(m_touchMode == TouchMode::Drawing) {
				touchMoveEvent(QDateTime::currentMSecsSinceEpoch(), posf);
			} else { // Shouldn't happen, but we'll deal with it anyway.
				m_touchMode = TouchMode::Drawing;
				touchPressEvent(QDateTime::currentMSecsSinceEpoch(), posf);
			}
		} else {
			// This still might be the beginning of a multitouch operation.
			// If the finger didn't move enough of a distance and we didn't
			// buffer an excessive amount of touches yet. Buffer the touched
			// point and wait a bit more as to what's going to happen.
			bool shouldAppend =
				bufferCount < TOUCH_DRAW_BUFFER_COUNT &&
				QLineF(m_touchDrawBuffer.first().second, posf).length() <
					TOUCH_DRAW_DISTANCE;
			if(shouldAppend) {
				m_touchDrawBuffer.append(
					{QDateTime::currentMSecsSinceEpoch(), posf});
			} else {
				m_touchMode = TouchMode::Drawing;
				flushTouchDrawBuffer();
				touchMoveEvent(QDateTime::currentMSecsSinceEpoch(), posf);
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
			"touch_update x=%f y=%f penstate=%d touching=%d type=%d "
			"device=%s points=%s timestamp=%llu",
			center.x(), center.y(), int(m_penState), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));

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
		bool haveMultiTouch = pointsCount >= 2;
		bool havePinchOrTwist =
			haveMultiTouch && (m_enableTouchPinch || m_enableTouchTwist);
		bool havePan = havePinchOrTwist ||
					   ((m_enableTouchScroll || m_enableTouchDraw) &&
						(haveMultiTouch || !compat::isTouchPad(event)));
		if(havePan) {
			m_touching = true;
			qreal dx = center.x() - lastCenter.x();
			qreal dy = center.y() - lastCenter.y();
			scrollByF(-dx, -dy);
		}

		// Scaling and rotation with two fingers
		if(havePinchOrTwist) {
			m_touching = true;
			qreal startAvgDist = 0.0;
			qreal avgDist = 0.0;
			for(const compat::TouchPoint &tp : compat::touchPoints(*event)) {
				startAvgDist +=
					squareDist(compat::touchStartPos(tp) - startCenter);
				avgDist += squareDist(compat::touchPos(tp) - center);
			}
			startAvgDist = sqrt(startAvgDist);

			qreal touchZoom = zoom();
			if(m_enableTouchPinch) {
				avgDist = sqrt(avgDist);
				qreal dZoom = avgDist / startAvgDist;
				touchZoom = m_touchStartZoom * dZoom;
			}

			qreal touchRotation = rotation();
			if(m_enableTouchTwist) {
				QLineF l1(
					compat::touchStartPos(points.first()),
					compat::touchStartPos(points.last()));
				QLineF l2(
					compat::touchPos(points.first()),
					compat::touchPos(points.last()));
				qreal dAngle = l1.angle() - l2.angle();

				// Require a small nudge to activate rotation to avoid
				// rotating when the user just wanted to zoom. Also only
				// rotate when touch points start out far enough from each
				// other. Initial angle measurement is inaccurate when
				// touchpoints are close together.
				if(startAvgDist * devicePixelRatioF() > 80.0 &&
				   (qAbs(dAngle) > 3.0 || m_touchRotating)) {
					m_touchRotating = true;
					touchRotation = m_touchStartRotate + dAngle;
				}
			}

			{
				QScopedValueRollback<bool> rollback(m_blockNotices, true);
				setZoom(touchZoom);
				setRotation(touchRotation);
			}

			showTouchTransformNotice();
		}
	}
}

void CanvasController::handleTouchEnd(QTouchEvent *event, bool cancel)
{
	event->accept();
	const QList<compat::TouchPoint> &points = compat::touchPoints(*event);
	if(m_enableTouchDraw &&
	   ((m_touchMode == TouchMode::Unknown && !m_touchDrawBuffer.isEmpty()) ||
		m_touchMode == TouchMode::Drawing)) {
		DP_EVENT_LOG(
			"touch_draw_%s penstate=%d touching=%d type=%d device=%s "
			"points=%s timestamp=%llu",
			cancel ? "cancel" : "end", int(m_penState), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
		flushTouchDrawBuffer();
		touchReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(),
			compat::touchPos(compat::touchPoints(*event).first()));
	} else {
		DP_EVENT_LOG(
			"touch_%s penstate=%d touching=%d type=%d device=%s points=%s "
			"timestamp=%llu",
			cancel ? "cancel" : "end", int(m_penState), int(m_touching),
			compat::touchDeviceType(event),
			qUtf8Printable(compat::touchDeviceName(event)),
			qUtf8Printable(compat::debug(points)),
			qulonglong(event->timestamp()));
	}
	m_touching = false;
}

void CanvasController::handleGesture(QGestureEvent *event)
{
	const QPinchGesture *pinch =
		static_cast<const QPinchGesture *>(event->gesture(Qt::PinchGesture));
	bool hadPinchUpdate = false;
	if(pinch) {
		Qt::GestureState pinchState = pinch->state();
		QPinchGesture::ChangeFlags cf = pinch->changeFlags();
		DP_EVENT_LOG(
			"pinch state=0x%x, change=0x%x scale=%f rotation=%f penstate=%d "
			"touching=%d",
			unsigned(pinchState), unsigned(cf), pinch->totalScaleFactor(),
			pinch->totalRotationAngle(), int(m_penState), int(m_touching));

		switch(pinch->state()) {
		case Qt::GestureStarted:
			m_gestureStartZoom = m_zoom;
			m_gestureStartAngle = m_rotation;
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished:
			if((m_enableTouchScroll || m_enableTouchDraw) &&
			   cf.testFlag(QPinchGesture::CenterPointChanged)) {
				QPointF d = pinch->centerPoint() - pinch->lastCenterPoint();
				scrollByF(-d.x(), -d.y());
			}

			{
				QScopedValueRollback<bool> rollback(m_blockNotices, true);
				if(m_enableTouchPinch &&
				   cf.testFlag(QPinchGesture::ScaleFactorChanged)) {
					setZoom(m_gestureStartZoom * pinch->totalScaleFactor());
				}
				if(m_enableTouchTwist &&
				   cf.testFlag(QPinchGesture::RotationAngleChanged)) {
					setRotation(
						m_gestureStartAngle + pinch->totalRotationAngle());
				}
			}

			showTouchTransformNotice();
			hadPinchUpdate = true;
			Q_FALLTHROUGH();
		case Qt::GestureCanceled:
			event->accept();
			break;
		default:
			break;
		}
	}

	const QPanGesture *pan =
		static_cast<const QPanGesture *>(event->gesture(Qt::PanGesture));
	if(pan) {
		Qt::GestureState panState = pan->state();
		QPointF delta = pan->delta();
		DP_EVENT_LOG(
			"pan state=0x%x dx=%f dy=%f penstate=%d touching=%d",
			unsigned(panState), delta.x(), delta.y(), int(m_penState),
			int(m_touching));

		switch(pan->state()) {
		case Qt::GestureStarted:
			m_gestureStartZoom = m_zoom;
			m_gestureStartAngle = m_rotation;
			Q_FALLTHROUGH();
		case Qt::GestureUpdated:
		case Qt::GestureFinished:
			if(!hadPinchUpdate && (m_enableTouchScroll || m_enableTouchDraw)) {
				scrollByF(delta.x() / -2.0, delta.y() / -2.0);
			}
			Q_FALLTHROUGH();
		case Qt::GestureCanceled:
			event->accept();
			break;
		default:
			break;
		}
	}
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
		int(m_penState), int(m_touching));

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
	case CanvasShortcuts::CANVAS_ZOOM: {
		event->accept();
		m_zoomWheelDelta += deltaY;
		int steps = m_zoomWheelDelta / 120;
		m_zoomWheelDelta -= steps * 120;
		if(steps != 0) {
			if(action == CanvasShortcuts::CANVAS_ROTATE) {
				setRotation(rotation() + steps * 10);
			} else if(action == CanvasShortcuts::CANVAS_ROTATE_DISCRETE) {
				rotateByDiscreteSteps(steps);
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
		event->accept();
		if(m_allowToolAdjust) {
			emit quickAdjust(deltaY / (30 * 4.0));
		}
		break;
	default:
		qWarning("Unhandled mouse wheel canvas shortcut %u", match.action());
		break;
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
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragModifiers = mouseMatch.shortcut->mods;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			updateOutline();
			resetCursor();
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
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_penMode = PenMode::Normal;
			m_dragMode = ViewDragMode::Started;
			m_dragAction = keyMatch.action();
			m_dragButton = Qt::NoButton;
			m_dragModifiers = keyMatch.shortcut->mods;
			m_dragInverted = keyMatch.inverted();
			m_dragSwapAxes = keyMatch.swapAxes();
			m_dragLastPoint = mapPointFromGlobal(QCursor::pos());
			m_dragCanvasPoint = mapPointToCanvasF(m_dragLastPoint);
			m_dragDiscreteRotation = 0.0;
			updateOutline();
			resetCursor();
			break;
		default:
			break;
		}

		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			modifiers, m_keysDown, Qt::LeftButton);
		switch(mouseMatch.action()) {
		case CanvasShortcuts::TOOL_ADJUST:
			if(!m_allowToolAdjust) {
				m_penMode = PenMode::Normal;
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ZOOM:
			m_penMode = PenMode::Normal;
			m_dragMode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragModifiers = mouseMatch.shortcut->mods;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
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
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragMode = ViewDragMode::None;
			updateOutline();
			resetCursor();
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
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragMode = ViewDragMode::Started;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragModifiers = mouseMatch.shortcut->mods;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			m_dragDiscreteRotation = 0.0;
			updateOutline();
			resetCursor();
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
			if(!m_allowToolAdjust) {
				m_dragMode = ViewDragMode::None;
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ZOOM:
			m_dragMode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragModifiers = mouseMatch.shortcut->mods;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
		default:
			m_dragMode = ViewDragMode::None;
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
	}

	updateOutline();
	resetCursor();
}

void CanvasController::flushTouchDrawBuffer()
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

void CanvasController::setEnableTouchScroll(bool enableTouchScroll)
{
	m_enableTouchScroll = enableTouchScroll;
}

void CanvasController::setEnableTouchDraw(bool enableTouchDraw)
{
	m_enableTouchDraw = enableTouchDraw;
}

void CanvasController::setEnableTouchPinch(bool enableTouchPinch)
{
	m_enableTouchPinch = enableTouchPinch;
}

void CanvasController::setEnableTouchTwist(bool enableTouchTwist)
{
	m_enableTouchTwist = enableTouchTwist;
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

void CanvasController::setBusy(bool busy)
{
	if(busy != m_busy) {
		m_busy = busy;
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
	bool fractionalTool)
{
	m_allowColorPick = allowColorPick;
	m_allowToolAdjust = allowToolAdjust;
	m_toolHandlesRightClick = toolHandlesRightClick;
	m_fractionalTool = fractionalTool;
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

void CanvasController::penMoveEvent(
	long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::KeyboardModifiers modifiers)
{
	canvas::Point point =
		mapPenPointToCanvasF(timeMsec, posf, pressure, xtilt, ytilt, rotation);
	emit coordinatesChanged(point);

	m_scene->setCursorPos(posf);

	if(m_dragMode == ViewDragMode::Started) {
		moveDrag(posf.toPoint());
	} else {
		if(m_fractionalTool || !m_prevPoint.intSame(point)) {
			if(m_penState == PenState::Up) {
				emit penHover(point, m_rotation, m_zoom, m_mirror, m_flip);
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
						m_canvasModel->pickColor(point.x(), point.y(), 0, 0);
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
			emit toggleActionActivated(action);
			m_hoveringOverHud = false;
			m_scene->removeHover();
			resetCursor();
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
			if(!m_allowToolAdjust) {
				break;
			}
			Q_FALLTHROUGH();
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ZOOM:
			if(m_dragMode != ViewDragMode::Started) {
				m_dragMode = ViewDragMode::Prepared;
				m_dragAction = match.action();
				m_dragButton = match.shortcut->button;
				m_dragModifiers = match.shortcut->mods;
				m_dragInverted = match.inverted();
				m_dragSwapAxes = match.swapAxes();
				updateOutline();
			}
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
			m_dragMode = ViewDragMode::Started;
			m_dragButton = button;
			m_dragModifiers = modifiers;
			m_dragDiscreteRotation = 0.0;
			resetCursor();

		} else if(
			(button == Qt::LeftButton || button == Qt::RightButton) &&
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
					if(!m_locked)
						emit penDown(
							point.timeMsec(), point, point.pressure(),
							point.xtilt(), point.ytilt(), point.rotation(),
							button == Qt::RightButton, m_rotation, m_zoom,
							m_mirror, m_flip, posf, deviceType, eraserOverride);
					break;
				case PenMode::Colorpick:
					m_canvasModel->pickColor(point.x(), point.y(), 0, 0);
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
		case CanvasShortcuts::CANVAS_ZOOM:
		case CanvasShortcuts::TOOL_ADJUST:
			m_dragMode = ViewDragMode::Prepared;
			m_dragAction = mouseMatch.action();
			m_dragButton = mouseMatch.shortcut->button;
			m_dragModifiers = mouseMatch.shortcut->mods;
			m_dragInverted = mouseMatch.inverted();
			m_dragSwapAxes = mouseMatch.swapAxes();
			break;
		default:
			m_dragMode = ViewDragMode::None;
			break;
		}

	} else if(
		m_penState == PenState::TabletDown ||
		((button == Qt::LeftButton || button == Qt::RightButton) &&
		 m_penState == PenState::MouseDown)) {

		if(!m_locked && m_penMode == PenMode::Normal) {
			emit penUp();
		}
		m_penState = PenState::Up;

		m_hoveringOverHud = m_scene->checkHover(posf.toPoint()) !=
							int(drawingboard::ToggleItem::Action::None);
		if(!m_hoveringOverHud) {
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST:
				if(!m_allowToolAdjust) {
					m_penMode = PenMode::Normal;
					break;
				}
				Q_FALLTHROUGH();
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ZOOM:
				m_penMode = PenMode::Normal;
				m_dragMode = ViewDragMode::Prepared;
				m_dragAction = mouseMatch.action();
				m_dragButton = mouseMatch.shortcut->button;
				m_dragModifiers = mouseMatch.shortcut->mods;
				m_dragInverted = mouseMatch.inverted();
				m_dragSwapAxes = mouseMatch.swapAxes();
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

void CanvasController::touchPressEvent(long long timeMsec, const QPointF &posf)
{
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
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE: {
		QSizeF halfSize = viewSizeF() / 2.0;
		qreal hw = halfSize.width();
		qreal hh = halfSize.height();
		qreal a1 = qAtan2(hw - m_dragLastPoint.x(), hh - m_dragLastPoint.y());
		qreal a2 = qAtan2(hw - point.x(), hh - point.y());
		qreal ad = qRadiansToDegrees(a1 - a2);
		qreal r = m_dragInverted ? -ad : ad;
		if(m_dragAction == CanvasShortcuts::CANVAS_ROTATE) {
			setRotation(rotation() + r);
		} else {
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
			setViewportCursor(m_rotateCursor);
			break;
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			setViewportCursor(m_rotateDiscreteCursor);
			break;
		case CanvasShortcuts::CANVAS_ZOOM:
			setViewportCursor(m_zoomCursor);
			break;
		case CanvasShortcuts::TOOL_ADJUST:
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
	} else if(m_busy) {
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
			 ? m_penMode == PenMode::Normal && !m_locked && !m_busy
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
	m_scene->setZoom(m_zoom);
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
	matrix.translate(-pos.x(), -pos.y());
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
			setZoomAt(scale, center);
			setRotation(rotation);
			setMirror(mirror);
			setFlip(flip);
		}

		showTransformNotice(getZoomNoticeText());
		emitTransformChanged();
	}
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
	if(m_enableTouchPinch) {
		if(m_enableTouchTwist) {
			showTransformNotice(QStringLiteral("%1\n%2").arg(
				getZoomNoticeText(), getRotationNoticeText()));
		} else {
			showTransformNotice(getZoomNoticeText());
		}
	} else if(m_enableTouchTwist) {
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
