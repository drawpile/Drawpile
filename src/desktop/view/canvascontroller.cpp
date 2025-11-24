// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/view/canvascontroller.h"
#include "desktop/main.h"
#include "desktop/scene/hudhandler.h"
#include "desktop/settings.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/view/canvasinterface.h"
#include "desktop/view/canvasscene.h"
#include "desktop/view/widgettouchhandler.h"

namespace view {

CanvasController::CanvasController(CanvasScene *scene, QObject *parent)
	: CanvasControllerBase(parent)
	, m_scene(scene)
	, m_touchHandler(new WidgetTouchHandler(this))
	, m_currentCursor(Qt::BlankCursor)
{
	DrawpileApp &app = dpApp();
	connect(
		&app, &DrawpileApp::tabletDriverChanged, this,
		&CanvasController::resetTabletDriver, Qt::QueuedConnection);
#if !defined(Q_OS_ANDROID) && !defined(__EMSCRIPTEN__)
	connect(
		&app, &DrawpileApp::eraserNear, this,
		&CanvasController::setEraserTipActive);
#endif

	desktop::settings::Settings &settings = app.settings();
	settings.bindCanvasViewBackgroundColor(
		this, &CanvasController::setClearColor);
	settings.bindRenderSmooth(this, &CanvasController::setRenderSmooth);
	settings.bindTabletEvents(this, &CanvasController::setTabletEnabled);
	settings.bindBrushOutlineWidth(this, &CanvasController::setOutlineWidth);
	settings.bindGlobalPressureCurve(
		this, &CanvasController::setSerializedPressureCurve);
	settings.bindGlobalPressureCurveEraser(
		this, &CanvasController::setSerializedPressureCurveEraser);
	settings.bindGlobalPressureCurveMode(
		this, &CanvasController::setPressureCurveMode);
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
		m_scene->hud(), &HudHandler::currentActionBarChanged, this,
		&CanvasController::clearHudHover);

	init();
}

void CanvasController::handleGesture(QGestureEvent *event)
{
	m_touchHandler->handleGesture(event, rawZoom(), rawRotation());
}

void CanvasController::setLockState(
	QFlags<view::Lock::Reason> reasons, const QStringList &descriptions,
	const QVector<QAction *> &actions)
{
	bool locked = reasons;
	setLocked(locked, descriptions, actions);
}

TouchHandler *CanvasController::touchHandler() const
{
	return m_touchHandler;
}

void CanvasController::setCursorOnCanvas(bool cursorOnCanvas)
{
	m_scene->setCursorOnCanvas(cursorOnCanvas);
}

void CanvasController::setCursorPos(const QPointF &posf)
{
	m_scene->setCursorPos(posf);
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

void CanvasController::showColorPick(int source, const QPointF &posf)
{
	m_scene->showColorPick(source, posf);
}

void CanvasController::hideColorPick()
{
	m_scene->hideColorPick();
}

HudAction CanvasController::checkHudHover(const QPointF &posf)
{
	return m_scene->hud()->checkHover(posf);
}

void CanvasController::activateHudAction(
	const HudAction &action, const QPoint &globalPos)
{
	m_scene->hud()->activateHudAction(action, globalPos);
}

void CanvasController::removeHudHover()
{
	m_scene->hud()->removeHover();
}

void CanvasController::updateSceneTransform(const QTransform &transform)
{
	m_scene->setCanvasTransform(transform);
	m_scene->setZoom(rawZoom() / devicePixelRatioF());
}

bool CanvasController::showHudTransformNotice(const QString &text)
{
	return m_scene->hud()->showTransformNotice(text);
}

bool CanvasController::showHudLockStatus(
	const QString &description, const QVector<QAction *> &actions)
{
	return m_scene->hud()->showLockStatus(description, actions);
}

bool CanvasController::hideHudLockStatus()
{
	return m_scene->hud()->hideLockStatus();
}

void CanvasController::handleViewSizeChange()
{
	QSizeF view = viewSizeF();
	m_scene->setSceneRect(0.0, 0.0, view.width(), view.height());
}

QPoint CanvasController::mapPointFromGlobal(const QPoint &point) const
{
	return m_canvasWidget ? m_canvasWidget->asWidget()->mapFromGlobal(point)
						  : point;
}

QSize CanvasController::viewSize() const
{
	return m_canvasWidget ? m_canvasWidget->viewSize() : QSize();
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

QPointF CanvasController::viewCursorPosOrCenter() const
{
	return m_scene->isCursorOnCanvas() ? m_scene->cursorPos() : viewCenterF();
}

}
