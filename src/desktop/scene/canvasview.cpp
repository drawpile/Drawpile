// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/blend_mode.h>
}
#include "desktop/main.h"
#include "desktop/scene/canvasscene.h"
#include "desktop/scene/canvasview.h"
#include "desktop/scene/toggleitem.h"
#include "desktop/tabletinput.h"
#include "desktop/utils/qtguicompat.h"
#include "desktop/utils/touchhandler.h"
#include "desktop/view/cursor.h"
#include "desktop/widgets/notifbar.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/drawdance/eventlog.h"
#include "libclient/tools/enums.h"
#include "libclient/tools/toolstate.h"
#include "libclient/utils/cursors.h"
#include "libshared/util/qtcompat.h"
#include <QApplication>
#include <QDateTime>
#include <QGestureEvent>
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
using utils::Cursors;

namespace widgets {

class CanvasView::SetDragParams {
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

CanvasView::CanvasView(QWidget *parent)
	: QGraphicsView(parent)
	, m_pendown(NOTDOWN)
	, m_allowColorPick(false)
	, m_allowToolAdjust(false)
	, m_toolHandlesRightClick(false)
	, m_fractionalTool(false)
	, m_penmode(PenMode::Normal)
	, m_tabletEventTimerDelay(0)
	, m_dragmode(ViewDragMode::None)
	, m_dragAction(CanvasShortcuts::NO_ACTION)
	, m_dragButton(Qt::NoButton)
	, m_dragInverted(false)
	, m_dragSwapAxes(false)
	, m_outlineSize(2)
	, m_showoutline(false)
	, m_subpixeloutline(true)
	, m_forceoutline(false)
	, m_pos{0.0, 0.0}
	, m_zoom(1.0)
	, m_rotate(0)
	, m_flip(false)
	, m_mirror(false)
	, m_scene(nullptr)
	, m_touch(new TouchHandler(this))
	, m_zoomWheelDelta(0)
	, m_enableTablet(true)
	, m_locked(false)
	, m_toolState(int(tools::ToolState::Normal))
	, m_saveInProgress(false)
	, m_pointertracking(false)
	, m_pixelgrid(true)
	, m_dpi(96)
	, m_brushCursorStyle(int(view::Cursor::TriangleRight))
	, m_eraseCursorStyle(int(view::Cursor::SameAsBrush))
	, m_alphaLockCursorStyle(int(view::Cursor::SameAsBrush))
	, m_brushOutlineWidth(1.0)
	, m_brushBlendMode(DP_BLEND_MODE_NORMAL)
	, m_hudActionToActivate(int(drawingboard::ToggleItem::Action::None))
	, m_scrollBarsAdjusting{false}
	, m_blockNotices{false}
	, m_showTransformNotices(true)
	, m_hoveringOverHud{false}
	, m_renderSmooth(false)
#ifdef Q_OS_LINUX
	, m_waylandWorkarounds(
		  QGuiApplication::platformName() == QStringLiteral("wayland"))
#endif
{
	viewport()->setAcceptDrops(true);
	viewport()->setAttribute(Qt::WA_AcceptTouchEvents);
	viewport()->setMouseTracking(true);
	setAcceptDrops(true);
	setFrameShape(QFrame::NoFrame);

	DrawpileApp &app = dpApp();
	connect(
		&app, &DrawpileApp::tabletDriverChanged, this,
		&CanvasView::resetTabletFilter, Qt::QueuedConnection);

	desktop::settings::Settings &settings = app.settings();
	settings.bindCanvasViewBackgroundColor(this, [this](QColor color) {
		color.setAlpha(255);
		setBackgroundBrush(color);
		update();
	});

	m_notificationBar = new NotificationBar(this);
	connect(
		m_notificationBar, &NotificationBar::actionButtonClicked, this,
		&CanvasView::activateNotificationBarAction);
	connect(
		m_notificationBar, &NotificationBar::closeButtonClicked, this,
		&CanvasView::dismissNotificationBar);

	settings.bindTabletEvents(this, &widgets::CanvasView::setTabletEnabled);
	settings.bindTouchGestures(
		this, &widgets::CanvasView::setTouchUseGestureEvents);
	settings.bindRenderSmooth(this, &widgets::CanvasView::setRenderSmooth);
	settings.bindRenderUpdateFull(
		this, &widgets::CanvasView::setRenderUpdateFull);
	settings.bindBrushCursor(this, &widgets::CanvasView::setBrushCursorStyle);
	settings.bindEraseCursor(this, &widgets::CanvasView::setEraseCursorStyle);
	settings.bindAlphaLockCursor(
		this, &widgets::CanvasView::setAlphaLockCursorStyle);
	settings.bindBrushOutlineWidth(
		this, &widgets::CanvasView::setBrushOutlineWidth);
	settings.bindTabletPressTimerDelay(
		this, &CanvasView::setTabletEventTimerDelay);

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

	settings.bindShowTransformNotices(
		this, &CanvasView::setShowTransformNotices);

	connect(
		m_touch, &TouchHandler::touchPressed, this,
		&CanvasView::touchPressEvent, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchMoved, this, &CanvasView::touchMoveEvent,
		Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchReleased, this,
		&CanvasView::touchReleaseEvent, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchScrolledBy, this, &CanvasView::scrollByF,
		Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchZoomedRotated, this,
		&CanvasView::touchZoomRotate, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchTapActionActivated, this,
		&CanvasView::touchTapActionActivated, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchColorPicked, this,
		&CanvasView::touchColorPick, Qt::DirectConnection);
	connect(
		m_touch, &TouchHandler::touchColorPickFinished, this,
		&CanvasView::hideSceneColorPick, Qt::DirectConnection);
}


void CanvasView::setTouchUseGestureEvents(bool useGestureEvents)
{
	if(useGestureEvents && !m_useGestureEvents) {
		DP_EVENT_LOG("grab gesture events");
		viewport()->grabGesture(Qt::TapGesture);
		viewport()->grabGesture(Qt::PanGesture);
		viewport()->grabGesture(Qt::PinchGesture);
		m_useGestureEvents = true;
	} else if(!useGestureEvents && m_useGestureEvents) {
		DP_EVENT_LOG("ungrab gesture events");
		viewport()->ungrabGesture(Qt::TapGesture);
		viewport()->ungrabGesture(Qt::PanGesture);
		viewport()->ungrabGesture(Qt::PinchGesture);
		m_useGestureEvents = false;
	}
}

void CanvasView::setRenderSmooth(bool smooth)
{
	if(smooth != m_renderSmooth) {
		m_renderSmooth = smooth;
		updateRenderHints();
		repaint();
	}
}

void CanvasView::setRenderUpdateFull(bool updateFull)
{
	ViewportUpdateMode mode =
		updateFull ? FullViewportUpdate : MinimalViewportUpdate;
	if(mode != viewportUpdateMode()) {
		setViewportUpdateMode(mode);
		repaint();
	}
}

void CanvasView::showDisconnectedWarning(
	const QString &message, bool singleSession)
{
	if(m_notificationBarState != NotificationBarState::Reconnect) {
		dismissNotificationBar();
		m_notificationBarState = NotificationBarState::Reconnect;
		m_notificationBar->show(
			message, QIcon::fromTheme("view-refresh"), tr("Reconnect"),
			NotificationBar::RoleColor::Warning, !singleSession);
		m_notificationBar->setActionButtonEnabled(true);
	}
}

void CanvasView::hideDisconnectedWarning()
{
	if(m_notificationBarState == NotificationBarState::Reconnect) {
		m_notificationBarState = NotificationBarState::None;
		m_notificationBar->hide();
	}
}

void CanvasView::showResetNotice(bool compatibilityMode, bool saveInProgress)
{
	if(m_notificationBarState != NotificationBarState::Reset) {
		dismissNotificationBar();
		m_notificationBarState = NotificationBarState::Reset;
		QString message =
			compatibilityMode
				? tr("Do you want to save the canvas as it was before the "
					 "reset? Since this is a Drawpile 2.1 session, it may have "
					 "desynchronized!")
				: tr("Do you want to save the canvas as it was before the "
					 "reset?");
		m_notificationBar->show(
			message, QIcon::fromTheme("document-save-as"), tr("Save As…"),
			NotificationBar::RoleColor::Notice);
		m_notificationBar->setActionButtonEnabled(!saveInProgress);
	}
}

void CanvasView::hideResetNotice()
{
	if(m_notificationBarState == NotificationBarState::Reset) {
		m_notificationBarState = NotificationBarState::None;
		m_notificationBar->hide();
	}
}

void CanvasView::activateNotificationBarAction()
{
	switch(m_notificationBarState) {
	case NotificationBarState::Reconnect:
		hideDisconnectedWarning();
		emit reconnectRequested();
		break;
	case NotificationBarState::Reset:
		emit savePreResetStateRequested();
		break;
	default:
		qWarning(
			"Unknown notification bar state %d", int(m_notificationBarState));
		break;
	}
}

void CanvasView::dismissNotificationBar()
{
	m_notificationBar->hide();
	NotificationBarState state = m_notificationBarState;
	m_notificationBarState = NotificationBarState::None;
	switch(state) {
	case NotificationBarState::None:
	case NotificationBarState::Reconnect:
		break;
	case NotificationBarState::Reset:
		emit savePreResetStateDismissed();
		break;
	default:
		qWarning("Unknown notification bar state %d", int(state));
		break;
	}
}

void CanvasView::setSaveInProgress(bool saveInProgress)
{
	m_saveInProgress = saveInProgress;
	m_notificationBar->setActionButtonEnabled(
		m_notificationBarState != NotificationBarState::Reset ||
		!saveInProgress);
	updateLockNotice();
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
			// Hack: on some systems, repainting doesn't work properly when the
			// canvas size is reduced, leaving bits of the prior canvas strewn
			// about. So we force the background to repaint by changing its
			// color slightly, actually forcing a refresh.
			QBrush bb = backgroundBrush();
			QColor c = bb.color();
			int red = c.red();
			c.setRed(red < 127 ? red + 1 : red - 1);
			setBackgroundBrush(c);
			repaint();
			setBackgroundBrush(bb);
		});
	connect(
		this, &CanvasView::viewRectChange, scene,
		&drawingboard::CanvasScene::canvasViewportChanged);
	connect(
		m_notificationBar, &NotificationBar::heightChanged, m_scene,
		&drawingboard::CanvasScene::setNotificationBarHeight);

	viewRectChanged();
	updateLockNotice();
}

void CanvasView::scrollBy(int x, int y)
{
	scrollByF(x, y);
}

void CanvasView::scrollByF(qreal x, qreal y)
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


void CanvasView::scrollStepLeft()
{
	scrollBy(-horizontalScrollBar()->singleStep(), 0);
}

void CanvasView::scrollStepRight()
{
	scrollBy(horizontalScrollBar()->singleStep(), 0);
}

void CanvasView::scrollStepUp()
{
	scrollBy(0, -verticalScrollBar()->singleStep());
}

void CanvasView::scrollStepDown()
{
	scrollBy(0, verticalScrollBar()->singleStep());
}

void CanvasView::zoominCenter()
{
	zoomStepsAt(1, mapToCanvas(rect().center()));
}

void CanvasView::zoominCursor()
{
	if(m_scene && m_scene->isCursorOnCanvas()) {
		zoomStepsAt(1, mapToCanvas(mapFromScene(m_scene->cursorPos())));
	} else {
		zoominCenter();
	}
}

void CanvasView::zoomoutCenter()
{
	zoomStepsAt(-1, mapToCanvas(rect().center()));
}

void CanvasView::zoomoutCursor()
{
	if(m_scene && m_scene->isCursorOnCanvas()) {
		zoomStepsAt(-1, mapToCanvas(mapFromScene(m_scene->cursorPos())));
	} else {
		zoomoutCenter();
	}
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
		qreal dpr = devicePixelRatioF();
		QRectF r = QRectF(QPointF(), QSizeF(m_scene->model()->size()) / dpr);
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
		setZoomAt(scale, m_pos * dpr);
		setRotation(rotate);
		setViewMirror(mirror);
		setViewFlip(flip);
	}
}

void CanvasView::setZoom(qreal zoom)
{
	setZoomAt(zoom, mapToCanvas(rect().center()));
}

void CanvasView::resetZoomCenter()
{
	setZoom(1.0);
}

void CanvasView::resetZoomCursor()
{
	if(m_scene && m_scene->isCursorOnCanvas()) {
		setZoomAt(1.0, mapToCanvas(mapFromScene(m_scene->cursorPos())));
	} else {
		resetZoomCenter();
	}
}

void CanvasView::setZoomAt(qreal zoom, const QPointF &point)
{
	qreal newZoom = qBound(zoomMin, zoom, zoomMax);
	if(newZoom != m_zoom) {
		QTransform matrix;
		mirrorFlip(matrix, m_mirror, m_flip);
		matrix.rotate(m_rotate);

		updateCanvasTransform([&] {
			m_pos +=
				matrix.map(point * (actualZoomFor(newZoom) - actualZoom()));
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

void CanvasView::resetRotation()
{
	setRotation(0.0);
}

void CanvasView::rotateStepClockwise()
{
	setRotation(rotation() + 5.0);
}

void CanvasView::rotateStepCounterClockwise()
{
	setRotation(rotation() - 5.0);
}

void CanvasView::setRotationSnap(qreal degrees)
{
	setRotation(qAbs(std::fmod(degrees, 360.0)) < 5.0 ? 0.0 : degrees);
}

void CanvasView::rotateByDiscreteSteps(int steps)
{
	constexpr qreal EPS = 0.01;
	constexpr qreal FULL = ROTATION_STEP_SIZE;
	constexpr qreal HALF = FULL / 2.0;

	// If we're not close to a discrete position, snap to it first.
	qreal offset = std::fmod(m_rotate, ROTATION_STEP_SIZE);
	if(steps < 0 && offset >= EPS && offset <= HALF) {
		setRotation(qFloor(m_rotate / FULL) * FULL);
		++steps;
	} else if(steps > 0 && offset >= HALF && offset <= FULL - EPS) {
		setRotation(qCeil(m_rotate / FULL) * FULL);
		--steps;
	}

	if(steps != 0) {
		setRotation((qRound(m_rotate / FULL) + steps) * FULL);
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

void CanvasView::setLockReasons(QFlags<view::Lock::Reason> reasons)
{
	m_locked = reasons;
	resetCursor();
}

void CanvasView::setLockDescription(const QString &lockDescription)
{
	m_lockDescription = lockDescription;
	updateLockNotice();
}

void CanvasView::setToolState(int toolState)
{
	m_toolState = toolState;
	resetCursor();
}

void CanvasView::setBrushCursorStyle(int style)
{
	m_brushCursorStyle = style;
	resetCursor();
}

void CanvasView::setEraseCursorStyle(int style)
{
	m_eraseCursorStyle = style;
	resetCursor();
}

void CanvasView::setAlphaLockCursorStyle(int style)
{
	m_alphaLockCursorStyle = style;
	resetCursor();
}

void CanvasView::setBrushOutlineWidth(qreal outlineWidth)
{
	m_brushOutlineWidth = qIsNaN(outlineWidth) ? 1.0 : outlineWidth;
	resetCursor();
}

void CanvasView::setBrushBlendMode(int brushBlendMode)
{
	m_brushBlendMode = brushBlendMode;
	resetCursor();
}

void CanvasView::setTabletEventTimerDelay(int tabletEventTimerDelay)
{
	m_tabletEventTimerDelay = tabletEventTimerDelay;
	m_tabletEventTimer.setRemainingTime(0);
}

void CanvasView::setShowTransformNotices(bool showTransformNotices)
{
	m_showTransformNotices = showTransformNotices;
}

#ifdef __EMSCRIPTEN__
void CanvasView::setEnableEraserOverride(bool enableEraserOverride)
{
	m_enableEraserOverride = enableEraserOverride;
}
#endif

void CanvasView::setToolCursor(const QCursor &cursor)
{
	m_toolcursor = cursor;
	resetCursor();
}

void CanvasView::setToolCapabilities(
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

void CanvasView::resetCursor()
{
	if(m_dragmode != ViewDragMode::None) {
		switch(m_dragAction) {
		case CanvasShortcuts::CANVAS_PAN:
			setViewportCursor(
				m_dragmode == ViewDragMode::Started ? Qt::ClosedHandCursor
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

	switch(m_penmode) {
	case PenMode::Normal:
		break;
	case PenMode::Colorpick:
		setViewportCursor(Cursors::colorPick());
		return;
	case PenMode::Layerpick:
		setViewportCursor(Cursors::layerPick());
		return;
	}

	if(m_locked) {
		setViewportCursor(Qt::ForbiddenCursor);
		return;
	} else if(m_toolState == int(tools::ToolState::Busy)) {
		setViewportCursor(Qt::WaitCursor);
		return;
	}

	if(m_toolcursor.shape() == Qt::CrossCursor) {
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
		default:
			setViewportCursor(Cursors::triangleRight());
			break;
		}
	} else {
		setViewportCursor(m_toolcursor);
	}
}

void CanvasView::setViewportCursor(const QCursor &cursor)
{
#ifdef HAVE_EMULATED_BITMAP_CURSOR
	viewport()->setCursor(
		cursor.shape() == Qt::BitmapCursor ? Qt::BlankCursor : cursor);
	if(m_scene) {
		m_scene->setCursor(cursor);
	}
#else
	viewport()->setCursor(cursor);
#endif
	updateOutline();
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
	m_outlineSize = newSize;
	updateOutline();
}

void CanvasView::setOutlineMode(bool subpixel, bool square, bool force)
{
	m_subpixeloutline = subpixel;
	m_forceoutline = force;
	if(m_scene) {
		m_scene->setOutlineTransform(getOutlinePos(), m_rotate);
		m_scene->setOutlineSquare(square);
		m_scene->setOutlineWidth(getOutlineWidth());
	}
}

bool CanvasView::activatePendingToggleAction()
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
	if(m_scene) {
		m_scene->setCursorOnCanvas(true);
	}

	// Give focus to this widget on mouseover. This is so that
	// using a key for dragging works rightaway. Avoid stealing
	// focus from text edit widgets though.
	QWidget *oldfocus = QApplication::focusWidget();
	if(!oldfocus ||
	   !(oldfocus->inherits("QLineEdit") || oldfocus->inherits("QTextEdit") ||
		 oldfocus->inherits("QPlainTextEdit"))) {
		setFocus(Qt::MouseFocusReason);
	}

	m_tabletFilter.reset();
}

void CanvasView::leaveEvent(QEvent *event)
{
	QGraphicsView::leaveEvent(event);
	if(m_scene) {
		m_scene->setCursorOnCanvas(false);
	}
	m_showoutline = false;
	m_hoveringOverHud = false;
	m_scene->removeHover();
	updateOutline();
	resetCursor();
	m_tabletFilter.reset();
}

void CanvasView::focusInEvent(QFocusEvent *event)
{
	QGraphicsView::focusInEvent(event);
	clearKeys();
}

void CanvasView::clearKeys()
{
	m_keysDown.clear();
	setDrag(SetDragParams::fromNone()
				.setPenMode(PenMode::Normal)
				.setUpdateOutline()
				.setResetCursor());
}

bool CanvasView::isTouchDrawEnabled() const
{
	return m_touch->isTouchDrawEnabled();
}

bool CanvasView::isTouchPanEnabled() const
{
	return m_touch->isTouchPanEnabled();
}

bool CanvasView::isTouchDrawOrPanEnabled() const
{
	return m_touch->isTouchDrawOrPanEnabled();
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
}

void CanvasView::onPenDown(
	const canvas::Point &p, bool right, const QPointF &viewPos,
	Qt::KeyboardModifiers modifiers, int deviceType, bool eraserOverride)
{
	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_locked) {
				CanvasShortcuts::ConstraintMatch constraintMatch =
					m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
				emit penDown(
					p.timeMsec(), p, p.pressure(), p.xtilt(), p.ytilt(),
					p.rotation(), right, m_rotate, m_zoom / devicePixelRatioF(),
					m_mirror, m_flip, constraintMatch.toolConstraint1(),
					constraintMatch.toolConstraint2(), viewPos, deviceType,
					eraserOverride);
			}
			break;
		case PenMode::Colorpick:
			pickColor(int(tools::ColorPickSource::Canvas), p, viewPos);
			break;
		case PenMode::Layerpick:
			m_scene->model()->pickLayer(p.x(), p.y());
			break;
		}
	}

	if(m_notificationBarState == NotificationBarState::Reset &&
	   m_notificationBar->isActionButtonEnabled()) {
		// There's a notice asking the user if they want to save the pre-reset
		// image, but they started drawing again, so apparently they don't want
		// to save it. Start a timer to automatically dismiss the notice.
		m_notificationBar->startAutoDismissTimer();
	}
}

void CanvasView::onPenMove(
	const canvas::Point &p, bool right, bool constrain1, bool constrain2,
	const QPointF &viewPos)
{
	Q_UNUSED(right)

	if(m_scene->hasImage()) {
		switch(m_penmode) {
		case PenMode::Normal:
			if(!m_locked)
				emit penMove(
					p.timeMsec(), p, p.pressure(), p.xtilt(), p.ytilt(),
					p.rotation(), constrain1, constrain2, viewPos);
			break;
		case PenMode::Colorpick:
			pickColor(int(tools::ColorPickSource::Canvas), p, viewPos);
			break;
		case PenMode::Layerpick:
			m_scene->model()->pickLayer(p.x(), p.y());
			break;
		}
	}
}

void CanvasView::startTabletEventTimer()
{
	m_touch->onTabletEventReceived();
	if(m_tabletEventTimerDelay > 0) {
		m_tabletEventTimer.setRemainingTime(m_tabletEventTimerDelay);
	}
}

void CanvasView::resetTabletFilter()
{
	m_tabletFilter.reset();
}

void CanvasView::penPressEvent(
	QEvent *event, long long timeMsec, const QPointF &pos, qreal pressure,
	qreal xtilt, qreal ytilt, qreal rotation, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers, int deviceType, bool eraserOverride)
{
	if(m_pendown != NOTDOWN ||
	   m_hudActionToActivate != int(drawingboard::ToggleItem::Action::None)) {
		return;
	}

	bool wasHovering;
	drawingboard::ToggleItem::Action action =
		m_scene->checkHover(mapToScene(pos.toPoint()), &wasHovering);
	m_hoveringOverHud = action != drawingboard::ToggleItem::Action::None;
	if(m_hoveringOverHud != wasHovering) {
		resetCursor();
	}
	if(m_hoveringOverHud) {
		if(event) {
			event->accept();
		}
		m_hudActionToActivate = int(action);
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
		setDrag(
			SetDragParams::fromMouseMatch(match)
				.setDragMode(ViewDragMode::Prepared)
				.setDragCondition(
					m_allowToolAdjust && m_dragmode != ViewDragMode::Started)
				.setUpdateOutline());
		break;
	case CanvasShortcuts::COLOR_H_ADJUST:
	case CanvasShortcuts::COLOR_S_ADJUST:
	case CanvasShortcuts::COLOR_V_ADJUST:
		setDrag(SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(
						m_allowColorPick && m_dragmode != ViewDragMode::Started)
					.setUpdateOutline());
		break;
	case CanvasShortcuts::CANVAS_PAN:
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
	case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
	case CanvasShortcuts::CANVAS_ZOOM:
		setDrag(SetDragParams::fromMouseMatch(match)
					.setDragMode(ViewDragMode::Prepared)
					.setDragCondition(m_dragmode != ViewDragMode::Started)
					.setUpdateOutline());
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
		m_dragCanvasPoint = mapToCanvas(m_dragLastPoint);
		m_dragmode = ViewDragMode::Started;
		m_dragButton = button;
		m_dragModifiers = modifiers;
		m_dragDiscreteRotation = 0.0;
		m_dragSnapRotation = this->rotation();
		resetCursor();
	} else if(
		(button == Qt::LeftButton || button == Qt::RightButton ||
		 penmode != PenMode::Normal) &&
		m_dragmode == ViewDragMode::None) {
		m_pendown = deviceType == int(tools::DeviceType::Tablet) ? TABLETDOWN
																 : MOUSEDOWN;
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
			button == Qt::RightButton, pos, modifiers, deviceType,
			eraserOverride);
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

static bool isSyntheticTouch(QMouseEvent *event)
{
#ifdef Q_OS_WIN
	// Windows may generate bogus mouse events from touches, we never want this.
	return event->source() & Qt::MouseEventSynthesizedBySystem;
#else
	Q_UNUSED(event);
	return false;
#endif
}

//! Handle mouse press events
void CanvasView::mousePressEvent(QMouseEvent *event)
{
	const auto mousePos = compat::mousePos(*event);
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_press x=%d y=%d buttons=0x%x modifiers=0x%x source=0x%x "
		"pendown=%d touching=%d timestamp=%llu",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()), m_pendown,
		touching, qulonglong(event->timestamp()));

	updateCursorPos(mousePos);

	Qt::MouseButton button = event->button();
	if((m_enableTablet && isSynthetic(event)) || isSyntheticTouch(event) ||
	   touching ||
	   (button == Qt::LeftButton && !m_tabletEventTimer.hasExpired())) {
		return;
	}

	penPressEvent(
		event, QDateTime::currentMSecsSinceEpoch(), mousePos, 1.0, 0.0, 0.0,
		0.0, button, getMouseModifiers(event), int(tools::DeviceType::Mouse),
		false);
}

void CanvasView::penMoveEvent(
	long long timeMsec, const QPointF &pos, qreal pressure, qreal xtilt,
	qreal ytilt, qreal rotation, Qt::MouseButtons buttons,
	Qt::KeyboardModifiers modifiers)
{
	if(m_hudActionToActivate != int(drawingboard::ToggleItem::Action::None)) {
		return;
	}

	canvas::Point point =
		mapToCanvas(timeMsec, pos, pressure, xtilt, ytilt, rotation);
	emit coordinatesChanged(point);

	if(m_dragmode == ViewDragMode::Started) {
		moveDrag(pos.toPoint());

	} else {
		if(m_fractionalTool || !m_prevpoint.intSame(point)) {
			CanvasShortcuts::ConstraintMatch match =
				m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
			if(m_pendown) {
				m_pointervelocity = point.distance(m_prevpoint);
				m_pointerdistance += m_pointervelocity;
				onPenMove(
					point, buttons.testFlag(Qt::RightButton),
					match.toolConstraint1(), match.toolConstraint2(), pos);

			} else {
				emit penHover(
					point, m_rotate, m_zoom / devicePixelRatioF(), m_mirror,
					m_flip, match.toolConstraint1(), match.toolConstraint2());
				if(m_pointertracking && m_scene->hasImage()) {
					emit pointerMoved(point);
				}

				bool wasHovering;
				m_hoveringOverHud =
					m_scene->checkHover(
						mapToScene(pos.toPoint()), &wasHovering) !=
					drawingboard::ToggleItem::Action::None;
				if(m_hoveringOverHud != wasHovering) {
					resetCursor();
				}
			}
			m_prevpoint = point;
		}
		updateOutlinePos(point);
		updateOutline();
	}
}

//! Handle mouse motion events
void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	const auto mousePos = compat::mousePos(*event);
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_move x=%d y=%d buttons=0x%x modifiers=0x%x source=0x%x "
		"pendown=%d touching=%d timestamp=%llu",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()), m_pendown,
		touching, qulonglong(event->timestamp()));

	updateCursorPos(mousePos);

	if((m_enableTablet && isSynthetic(event)) || isSyntheticTouch(event) ||
	   m_pendown == TABLETDOWN || touching ||
	   (m_pendown && !m_tabletEventTimer.hasExpired())) {
		return;
	}

	if(m_pendown && event->buttons() == Qt::NoButton) {
		// In case we missed a mouse release
		mouseReleaseEvent(event);
		return;
	}

	penMoveEvent(
		QDateTime::currentMSecsSinceEpoch(), mousePos, 1.0, 0.0, 0.0, 0.0,
		event->buttons(), getMouseModifiers(event));
}

void CanvasView::penReleaseEvent(
	long long timeMsec, const QPointF &pos, Qt::MouseButton button,
	Qt::KeyboardModifiers modifiers)
{
	activatePendingToggleAction();

	canvas::Point point = mapToCanvas(timeMsec, pos, 0.0, 0.0, 0.0, 0.0);
	m_prevpoint = point;
	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		modifiers, m_keysDown, Qt::LeftButton);

	if(m_dragmode != ViewDragMode::None && m_dragButton != Qt::NoButton) {
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
		m_pendown == TABLETDOWN ||
		((button == Qt::LeftButton || button == Qt::RightButton) &&
		 m_pendown == MOUSEDOWN)) {
		if(!m_locked && m_penmode == PenMode::Normal) {
			CanvasShortcuts::ConstraintMatch constraintMatch =
				m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
			emit penUp(
				constraintMatch.toolConstraint1(),
				constraintMatch.toolConstraint2());
		}

		if(m_pickingColor) {
			hideSceneColorPick();
			m_pickingColor = false;
		}

		m_pendown = NOTDOWN;

		m_hoveringOverHud = m_scene->checkHover(mapToScene(pos.toPoint())) !=
							drawingboard::ToggleItem::Action::None;
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
	}

	updateOutlinePos(point);
	resetCursor();
}

void CanvasView::touchPressEvent(
	QEvent *event, long long timeMsec, const QPointF &pos)
{
	penPressEvent(
		event, timeMsec, pos, 1.0, 0.0, 0.0, 0.0, Qt::LeftButton,
		Qt::NoModifier, int(tools::DeviceType::Touch), false);
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

void CanvasView::touchZoomRotate(qreal zoom, qreal rotation)
{
	{
		QScopedValueRollback<bool> rollback(m_blockNotices, true);
		setZoom(zoom);
		setRotation(rotation);
	}
	showTouchTransformNotice();
}

void CanvasView::gestureEvent(QGestureEvent *event)
{
	m_touch->handleGesture(event, m_zoom, m_rotate);
}

//! Handle mouse release events
void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	const auto mousePos = compat::mousePos(*event);
	bool touching = m_touch->isTouching();
	DP_EVENT_LOG(
		"mouse_release x=%d y=%d buttons=0x%x modifiers=0x%x source=0x%x "
		"pendown=%d touching=%d timestamp=%llu",
		mousePos.x(), mousePos.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), unsigned(event->source()), m_pendown,
		touching, qulonglong(event->timestamp()));

	updateCursorPos(mousePos);

	if((m_enableTablet && isSynthetic(event)) || isSyntheticTouch(event) ||
	   touching || !m_tabletEventTimer.hasExpired()) {
		return;
	}

	penReleaseEvent(
		QDateTime::currentMSecsSinceEpoch(), mousePos, event->button(),
		getMouseModifiers(event));
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
	mousePressEvent(event);
}

void CanvasView::wheelEvent(QWheelEvent *event)
{
	QPoint angleDelta = event->angleDelta();
	DP_EVENT_LOG(
		"wheel x=%d y=%d buttons=0x%x modifiers=0x%x pendown=%d touching=%d",
		angleDelta.x(), angleDelta.y(), unsigned(event->buttons()),
		unsigned(event->modifiers()), m_pendown, m_touch->isTouching());

	CanvasShortcuts::Match match =
		m_canvasShortcuts.matchMouseWheel(geWheelModifiers(event), m_keysDown);
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
				zoomStepsAt(steps, mapToCanvas(compat::wheelPosition(*event)));
			}
		}
		break;
	}
	// Color and layer picking by spinning the scroll wheel is weird, but okay.
	case CanvasShortcuts::COLOR_PICK:
		event->accept();
		if(m_allowColorPick && m_scene->hasImage()) {
			QPointF p = mapToCanvas(compat::wheelPosition(*event));
			m_scene->model()->pickColor(p.x(), p.y(), 0, 0);
		}
		break;
	case CanvasShortcuts::LAYER_PICK: {
		event->accept();
		if(m_scene->hasImage()) {
			QPointF p = mapToCanvas(compat::wheelPosition(*event));
			m_scene->model()->pickLayer(p.x(), p.y());
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

void CanvasView::wheelAdjust(
	QWheelEvent *event, int param, bool allowed, int delta)
{
	event->accept();
	if(allowed) {
		emit quickAdjust(param, qreal(delta) / 120.0);
	}
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
	DP_EVENT_LOG(
		"key_press key=%d modifiers=0x%x autorepeat=%d", event->key(),
		unsigned(event->modifiers()), event->isAutoRepeat());

	QGraphicsView::keyPressEvent(event);
	if(event->isAutoRepeat() || m_hoveringOverHud) {
		return;
	}

	m_keysDown.insert(Qt::Key(event->key()));

	if(m_dragmode == ViewDragMode::Started && m_dragButton != Qt::NoButton) {
		// There's currently some dragging with a mouse button held down going
		// on. Switch to a different flavor of drag if appropriate and bail out.
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			getKeyboardModifiers(event), m_keysDown, m_dragButton);
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
			setDrag(SetDragParams::fromMouseMatch(mouseMatch).setResetCursor());
			break;
		default:
			break;
		}
		return;
	}

	if(m_pendown == NOTDOWN) {
		CanvasShortcuts::Match keyMatch = m_canvasShortcuts.matchKeyCombination(
			getKeyboardModifiers(event), Qt::Key(event->key()));
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
						.setDragMode(ViewDragMode::Started)
						.setResetDragPoints()
						.setResetDragRotation()
						.setResetCursor());
			break;
		default:
			CanvasShortcuts::Match mouseMatch =
				m_canvasShortcuts.matchMouseButton(
					getKeyboardModifiers(event), m_keysDown, Qt::LeftButton);
			switch(mouseMatch.action()) {
			case CanvasShortcuts::TOOL_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowToolAdjust)
							.setUpdateOutline());
				if(!m_allowToolAdjust) {
					emitPenModify(getKeyboardModifiers(event));
				}
				break;
			case CanvasShortcuts::COLOR_H_ADJUST:
			case CanvasShortcuts::COLOR_S_ADJUST:
			case CanvasShortcuts::COLOR_V_ADJUST:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setDragCondition(m_allowColorPick)
							.setUpdateOutline());
				if(!m_allowColorPick) {
					emitPenModify(getKeyboardModifiers(event));
				}
				break;
			case CanvasShortcuts::CANVAS_PAN:
			case CanvasShortcuts::CANVAS_ROTATE:
			case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
			case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
			case CanvasShortcuts::CANVAS_ZOOM:
				setDrag(SetDragParams::fromMouseMatch(mouseMatch)
							.setPenMode(PenMode::Normal)
							.setDragMode(ViewDragMode::Prepared)
							.setUpdateOutline());
				break;
			case CanvasShortcuts::COLOR_PICK:
				m_penmode =
					m_allowColorPick ? PenMode::Colorpick : PenMode::Normal;
				break;
			case CanvasShortcuts::LAYER_PICK:
				m_penmode = PenMode::Layerpick;
				break;
			default:
				m_penmode = PenMode::Normal;
				emitPenModify(getKeyboardModifiers(event));
				break;
			}
			break;
		}
	} else {
		emitPenModify(getKeyboardModifiers(event));
		QGraphicsView::keyPressEvent(event);
	}

	resetCursor();
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
	DP_EVENT_LOG(
		"key_release key=%d modifiers=0x%x autorepeat=%d", event->key(),
		unsigned(event->modifiers()), event->isAutoRepeat());

	QGraphicsView::keyReleaseEvent(event);
	if(event->isAutoRepeat()) {
		return;
	}

	bool wasDragging = m_dragmode == ViewDragMode::Started;
	if(wasDragging) {
		CanvasShortcuts::Match dragMatch =
			m_dragButton == Qt::NoButton
				? m_canvasShortcuts.matchKeyCombination(
					  m_dragModifiers, Qt::Key(event->key()))
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
			updateOutlinePos(mapToCanvas(m_dragLastPoint));
			setDrag(SetDragParams::fromNone().setResetCursor());
			break;
		default:
			break;
		}
	}

	m_keysDown.remove(Qt::Key(event->key()));

	if(wasDragging && m_dragButton != Qt::NoButton) {
		CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
			getKeyboardModifiers(event), m_keysDown, m_dragButton);
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
						.setResetCursor());
			break;
		default:
			break;
		}
		return;
	}

	CanvasShortcuts::Match mouseMatch = m_canvasShortcuts.matchMouseButton(
		getKeyboardModifiers(event), m_keysDown, Qt::LeftButton);
	if(m_dragmode == ViewDragMode::Prepared) {
		switch(mouseMatch.action()) {
		case CanvasShortcuts::TOOL_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(m_allowToolAdjust)
					.setUpdateOutline());
			break;
		case CanvasShortcuts::COLOR_H_ADJUST:
		case CanvasShortcuts::COLOR_S_ADJUST:
		case CanvasShortcuts::COLOR_V_ADJUST:
			setDrag(
				SetDragParams::fromMouseMatch(mouseMatch)
					.setDragMode(ViewDragMode::Prepared)
					.setDragConditionClearDragModeOnFailure(m_allowColorPick)
					.setUpdateOutline());
			break;
		case CanvasShortcuts::CANVAS_PAN:
		case CanvasShortcuts::CANVAS_ROTATE:
		case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
		case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP:
		case CanvasShortcuts::CANVAS_ZOOM:
			setDrag(SetDragParams::fromMouseMatch(mouseMatch)
						.setDragMode(ViewDragMode::Prepared)
						.setUpdateOutline());
			break;
		default:
			setDrag(SetDragParams::fromNone());
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
			emitPenModify(getKeyboardModifiers(event));
			break;
		}
	} else {
		emitPenModify(getKeyboardModifiers(event));
	}

	resetCursor();
}

void CanvasView::setPressureCurve(const KisCubicCurve &pressureCurve)
{
	m_pressureCurve = pressureCurve;
}

void CanvasView::touchEvent(QTouchEvent *event)
{
	event->accept();

	switch(event->type()) {
	case QEvent::TouchBegin: {
		if(m_hudActionToActivate ==
		   int(drawingboard::ToggleItem::Action::None)) {
			const QList<compat::TouchPoint> &points =
				compat::touchPoints(*event);
			int pointsCount = points.size();
			if(pointsCount > 0) {
				QPointF pos = compat::touchPos(points.first());
				drawingboard::ToggleItem::Action action =
					m_scene->checkHover(mapToScene(pos.toPoint()));
				if(action == drawingboard::ToggleItem::Action::None) {
					m_touch->handleTouchBegin(event);
				} else {
					m_hudActionToActivate = int(action);
				}
			}
		}
		break;
	}
	case QEvent::TouchUpdate:
		if(m_hudActionToActivate ==
		   int(drawingboard::ToggleItem::Action::None)) {
			m_touch->handleTouchUpdate(
				event, zoom(), rotation(), devicePixelRatioF());
		}
		break;
	case QEvent::TouchEnd:
		if(!activatePendingToggleAction()) {
			m_touch->handleTouchEnd(event, false);
		}
		break;
	case QEvent::TouchCancel:
		if(!activatePendingToggleAction()) {
			m_touch->handleTouchEnd(event, true);
		}
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
	QEvent::Type type = event->type();
	if(type == QEvent::Gesture && m_useGestureEvents) {
		gestureEvent(static_cast<QGestureEvent *>(event));
	} else if(
		(type == QEvent::TouchBegin || type == QEvent::TouchUpdate ||
		 type == QEvent::TouchEnd || type == QEvent::TouchCancel) &&
		!m_useGestureEvents) {
		touchEvent(static_cast<QTouchEvent *>(event));
	} else if(type == QEvent::TabletPress && m_enableTablet) {
		startTabletEventTimer();

		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(tabev);
		bool ignore = m_tabletFilter.shouldIgnore(tabev);
		DP_EVENT_LOG(
			"tablet_press spontaneous=%d x=%f y=%f pressure=%f xtilt=%d "
			"ytilt=%d rotation=%f buttons=0x%x modifiers=0x%x pendown=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(tabev->spontaneous()), tabPos.x(), tabPos.y(),
			tabev->pressure(), compat::cast_6<int>(tabev->xTilt()),
			compat::cast_6<int>(tabev->yTilt()),
			qDegreesToRadians(tabev->rotation()), unsigned(tabev->buttons()),
			unsigned(tabev->modifiers()), m_pendown, m_touch->isTouching(),
			unsigned(modifiers), int(ignore));
		if(ignore) {
			return true;
		}

		Qt::MouseButton button;
		bool eraserOverride;
#ifdef __EMSCRIPTEN__
		// In the browser, we don't get eraser proximity events, instead an
		// eraser pressed down is reported as button flag 0x20 (Qt::TaskButton).
		if(tabev->buttons().testFlag(Qt::TaskButton)) {
			button = Qt::LeftButton;
			eraserOverride = m_enableEraserOverride;
		} else {
			button = tabev->button();
			eraserOverride = false;
		}
#else
		button = tabev->button();
		eraserOverride = false;
#endif

		updateCursorPos(tabPos.toPoint());

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
			event, QDateTime::currentMSecsSinceEpoch(), compat::tabPosF(*tabev),
			tabev->pressure(), tabev->xTilt(), tabev->yTilt(),
			qDegreesToRadians(tabev->rotation()), button, modifiers,
			int(tools::DeviceType::Tablet), eraserOverride);
	} else if(type == QEvent::TabletMove && m_enableTablet) {
		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(tabev);
		Qt::MouseButtons buttons = tabev->buttons();
		bool ignore = m_tabletFilter.shouldIgnore(tabev);
		DP_EVENT_LOG(
			"tablet_move spontaneous=%d x=%f y=%f pressure=%f xtilt=%d "
			"ytilt=%d rotation=%f buttons=0x%x modifiers=0x%x pendown=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(tabev->spontaneous()), tabPos.x(), tabPos.y(),
			tabev->pressure(), compat::cast_6<int>(tabev->xTilt()),
			compat::cast_6<int>(tabev->yTilt()),
			qDegreesToRadians(tabev->rotation()), unsigned(buttons),
			unsigned(tabev->modifiers()), m_pendown, m_touch->isTouching(),
			unsigned(modifiers), int(ignore));
		if(ignore) {
			return true;
		}

		if(!tabletinput::passPenEvents()) {
			tabev->accept();
		}

		// Under Windows Ink, some tablets report bogus zero-pressure inputs.
		// Buttons other than the left button (the pen tip) are expected to have
		// zero pressure, so we do handle those.
		if(!m_pendown || tabev->pressure() != 0.0 ||
		   buttons != Qt::LeftButton) {
			startTabletEventTimer();
			updateCursorPos(tabPos.toPoint());
			penMoveEvent(
				QDateTime::currentMSecsSinceEpoch(), compat::tabPosF(*tabev),
				tabev->pressure(), tabev->xTilt(), tabev->yTilt(),
				qDegreesToRadians(tabev->rotation()), tabev->buttons(),
				modifiers);
		}
	} else if(type == QEvent::TabletRelease && m_enableTablet) {
		startTabletEventTimer();

		QTabletEvent *tabev = static_cast<QTabletEvent *>(event);
		const auto tabPos = compat::tabPosF(*tabev);
		Qt::KeyboardModifiers modifiers = getTabletModifiers(tabev);
		bool ignore = m_tabletFilter.shouldIgnore(tabev);
		DP_EVENT_LOG(
			"tablet_release spontaneous=%d x=%f y=%f buttons=0x%x pendown=%d "
			"touching=%d effectivemodifiers=0x%u ignore=%d",
			int(tabev->spontaneous()), tabPos.x(), tabPos.y(),
			unsigned(tabev->buttons()), m_pendown, m_touch->isTouching(),
			unsigned(modifiers), int(ignore));
		if(ignore) {
			return true;
		}

		updateCursorPos(tabPos.toPoint());
		if(!tabletinput::passPenEvents()) {
			tabev->accept();
		}
		penReleaseEvent(
			QDateTime::currentMSecsSinceEpoch(), tabPos, tabev->button(),
			modifiers);
	} else {
		return QGraphicsView::viewportEvent(event);
	}

	return true;
}

void CanvasView::updateOutlinePos(QPointF point)
{
	if(m_showoutline && !m_locked &&
	   m_toolState == int(tools::ToolState::Normal) && !m_hoveringOverHud &&
	   (!canvas::Point::roughlySame(point, m_prevoutlinepoint))) {
		if(!m_subpixeloutline) {
			point.setX(qFloor(point.x()) + 0.5);
			point.setY(qFloor(point.y()) + 0.5);
		}
		m_prevoutlinepoint = point;
		updateOutline();
	}
}

void CanvasView::updateOutline()
{
	if(m_scene) {
		m_scene->setOutline(m_outlineSize * actualZoom(), getOutlineWidth());
		m_scene->setOutlineTransform(getOutlinePos(), m_rotate);
		m_scene->setOutlineVisibleInMode(
			!m_hoveringOverHud &&
			(m_dragmode == ViewDragMode::None
				 ? m_penmode == PenMode::Normal && !m_locked &&
					   m_toolState == int(tools::ToolState::Normal)
				 : m_dragAction == CanvasShortcuts::TOOL_ADJUST));
	}
}

QPointF CanvasView::getOutlinePos() const
{
	QPointF pos = mapFromCanvas(m_prevoutlinepoint);
	if(!m_subpixeloutline && m_outlineSize % 2 == 0) {
		qreal offset = actualZoom() * 0.5;
		pos -= QPointF(offset, offset);
	}
	return pos;
}

qreal CanvasView::getOutlineWidth() const
{
	return m_forceoutline ? qMax(1.0, m_brushOutlineWidth)
						  : m_brushOutlineWidth;
}

int CanvasView::getCurrentCursorStyle() const
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

void CanvasView::updateCursorPos(const QPoint &pos)
{
	if(m_scene) {
		m_scene->setCursorPos(mapToScene(pos));
	}
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

void CanvasView::setDrag(const SetDragParams &params)
{
	ViewDragMode prevMode = m_dragmode;
	CanvasShortcuts::Action prevAction = m_dragAction;

	if(params.hasPenMode()) {
		m_penmode = params.penMode();
	}

	if(params.isDragConditionFulfilled()) {
		bool shouldSetDragParams = true;
		if(params.hasDragMode()) {
			m_dragmode = params.dragMode();
			if(m_dragmode == ViewDragMode::None) {
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
				m_dragLastPoint = mapFromGlobal(QCursor::pos());
				m_dragCanvasPoint = mapToCanvas(m_dragLastPoint);
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

	if(m_dragmode == ViewDragMode::Started) {
		if(prevMode != ViewDragMode::Started &&
		   CanvasShortcuts::isColorAdjustAction(m_dragAction)) {
			showSceneColorPick(
				int(tools::ColorPickSource::Adjust), m_dragLastPoint);
		}
	} else if(
		prevMode == ViewDragMode::Started &&
		CanvasShortcuts::isColorAdjustAction(prevAction)) {
		m_scene->hideColorPick();
	}
}

void CanvasView::dragAdjust(int type, int delta, qreal acceleration)
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
	case CanvasShortcuts::CANVAS_ROTATE:
	case CanvasShortcuts::CANVAS_ROTATE_DISCRETE:
	case CanvasShortcuts::CANVAS_ROTATE_NO_SNAP: {
		qreal hw = width() / 2.0;
		qreal hh = height() / 2.0;
		qreal a1 = qAtan2(hw - m_dragLastPoint.x(), hh - m_dragLastPoint.y());
		qreal a2 = qAtan2(hw - point.x(), hh - point.y());
		qreal ad = qRadiansToDegrees(a1 - a2);
		qreal r = m_dragInverted ? -ad : ad;
		if(m_dragAction == CanvasShortcuts::CANVAS_ROTATE) {
			m_dragSnapRotation += r;
			setRotationSnap(m_dragSnapRotation);
		} else if(m_dragAction == CanvasShortcuts::CANVAS_ROTATE_DISCRETE) {
			m_dragDiscreteRotation += r;
			qreal discrete = m_dragDiscreteRotation / ROTATION_STEP_SIZE;
			if(discrete >= 1.0) {
				int steps = qFloor(discrete);
				m_dragDiscreteRotation -= steps * ROTATION_STEP_SIZE;
				rotateByDiscreteSteps(steps);
			} else if(discrete <= -1.0) {
				int steps = qCeil(discrete);
				m_dragDiscreteRotation += steps * -ROTATION_STEP_SIZE;
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
	}

	m_dragLastPoint = point;
}

bool CanvasView::showSceneColorPick(int source, const QPointF &posf)
{
	return m_scene->showColorPick(source, mapToScene(posf.toPoint()));
}

void CanvasView::hideSceneColorPick()
{
	m_scene->hideColorPick();
	m_pickingColor = false;
}

void CanvasView::pickColor(
	int source, const QPointF &point, const QPointF &posf)
{
	m_scene->model()->pickColor(point.x(), point.y(), 0, 0);
	m_pickingColor = showSceneColorPick(source, posf);
}

void CanvasView::touchColorPick(const QPointF &posf)
{
	pickColor(int(tools::ColorPickSource::Touch), mapToCanvas(posf), posf);
}

void CanvasView::updateCanvasTransform(const std::function<void()> &block)
{
	QPointF outlinePoint = fromCanvasTransform().map(m_prevoutlinepoint);

	block();

	if(m_scene) {
		updatePosBounds();
		QRectF rectBefore = m_scene->canvasBounds();
		m_scene->setCanvasTransform(calculateCanvasTransform());
		m_scene->setZoom(m_zoom / devicePixelRatioF());
		QRectF rectAfter = m_scene->canvasBounds();
		updateScrollBars();
		updateCanvasPixelGrid();
		updateRenderHints();
		viewRectChanged();
		updateScene({rectBefore, rectAfter});
	}

	m_prevoutlinepoint = toCanvasTransform().map(outlinePoint);
	updateOutline();
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

void CanvasView::updateRenderHints()
{
	// Use nearest-neighbor interpolation at 200% zoom and above, anything
	// below gets linear interpolation. An exception is at exactly 100% zoom
	// with a right-angle rotation, since otherwise the canvas gets blurred
	// even though the pixels are at a 1:1 scale. On Emscripten, this causes
	// immense slowdown, so we never enable linear interpolation there.
	bool smooth = m_renderSmooth && m_zoom <= 1.99 &&
				  !(qAbs(m_zoom - 1.0) < 0.01 &&
					std::fmod(qAbs(rotation()), 90.0) < 0.01);
	setRenderHint(QPainter::SmoothPixmapTransform, smooth);
}

QTransform CanvasView::calculateCanvasTransform() const
{
	return calculateCanvasTransformFrom(
		m_pos, m_zoom, m_rotate, m_mirror, m_flip);
}

QTransform CanvasView::calculateCanvasTransformFrom(
	const QPointF &pos, qreal zoom, qreal rotate, bool mirror, bool flip) const
{
	QTransform matrix;
	matrix.translate(-pos.x(), -pos.y());
	qreal scale = actualZoomFor(zoom);
	matrix.scale(scale, scale);
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

void CanvasView::emitPenModify(Qt::KeyboardModifiers modifiers)
{
	CanvasShortcuts::ConstraintMatch match =
		m_canvasShortcuts.matchConstraints(modifiers, m_keysDown);
	emit penModify(match.toolConstraint1(), match.toolConstraint2());
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
		updateOutlinePos(mapToCanvas(mapFromGlobal(QCursor::pos())));
		updateOutline();
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

void CanvasView::showTouchTransformNotice()
{
	if(m_touch->isTouchPinchEnabled()) {
		if(m_touch->isTouchTwistEnabled()) {
			showTransformNotice(QStringLiteral("%1\n%2").arg(
				getZoomNotice(), getRotationNotice()));
		} else {
			showTransformNotice(getZoomNotice());
		}
	} else if(m_touch->isTouchTwistEnabled()) {
		showTransformNotice(getRotationNotice());
	}
}

void CanvasView::showTransformNotice(const QString &text)
{
	if(m_scene && !m_blockNotices && m_showTransformNotices) {
		m_scene->showTransformNotice(text);
	}
}

void CanvasView::updateLockNotice()
{
	if(m_scene) {
		QStringList description;
		if(m_saveInProgress) {
#ifdef __EMSCRIPTEN__
			description.append(tr("Downloading…"));
#else
			description.append(tr("Saving…"));
#endif
		}
		if(!m_lockDescription.isEmpty()) {
			description.append(m_lockDescription);
		}

		if(description.isEmpty()) {
			m_scene->hideLockNotice();
		} else {
			m_scene->showLockNotice(description.join(QStringLiteral("\n")));
		}
	}
}

Qt::KeyboardModifiers
CanvasView::getKeyboardModifiers(const QKeyEvent *keyev) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(keyev);
	return getFallbackModifiers();
#else
	return keyev->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasView::getMouseModifiers(const QMouseEvent *mouseev) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(mouseev);
	return getFallbackModifiers();
#else
	return mouseev->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasView::getTabletModifiers(const QTabletEvent *tabev) const
{
#if defined(Q_OS_ANDROID) || defined(__EMSCRIPTEN__)
	// Qt always reports no modifiers on Android.
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(tabev);
	return getFallbackModifiers();
#else
#	ifdef Q_OS_LINUX
	// Tablet event modifiers aren't reported properly on Wayland.
	if(m_waylandWorkarounds) {
		return getFallbackModifiers();
	}
#	endif
	return tabev->modifiers();
#endif
}

Qt::KeyboardModifiers
CanvasView::geWheelModifiers(const QWheelEvent *wheelev) const
{
#ifdef __EMSCRIPTEN__
	// Modifiers reported on Emscripten are just complete garbage.
	Q_UNUSED(wheelev);
	return getFallbackModifiers();
#else
	return wheelev->modifiers();
#endif
}

Qt::KeyboardModifiers CanvasView::getFallbackModifiers() const
{
	Qt::KeyboardModifiers mods;
	mods.setFlag(Qt::ControlModifier, m_keysDown.contains(Qt::Key_Control));
	mods.setFlag(Qt::ShiftModifier, m_keysDown.contains(Qt::Key_Shift));
	mods.setFlag(Qt::AltModifier, m_keysDown.contains(Qt::Key_Alt));
	mods.setFlag(Qt::MetaModifier, m_keysDown.contains(Qt::Key_Meta));
	return mods;
}

}
