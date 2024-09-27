// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_VIEW_CANVASCONTROLLER_H
#define DESKTOP_VIEW_CANVASCONTROLLER_H
#include "desktop/view/lock.h"
#include "libclient/canvas/canvasshortcuts.h"
#include "libclient/canvas/point.h"
#include "libclient/utils/kis_cubic_curve.h"
#include <QColor>
#include <QCursor>
#include <QDeadlineTimer>
#include <QObject>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QSet>
#include <QSize>
#include <QTransform>
#include <QVector>
#include <functional>

class QGestureEvent;
class QKeyEvent;
class QMouseEvent;
class QTabletEvent;
class QTouchEvent;
class QWheelEvent;
class TouchHandler;

namespace canvas {
class CanvasModel;
class TileCache;
}

namespace view {

class CanvasInterface;
class CanvasScene;

class CanvasController : public QObject {
	Q_OBJECT
public:
	explicit CanvasController(CanvasScene *scene, QWidget *parent);

	CanvasScene *scene() const { return m_scene; }

	void setCanvasWidget(CanvasInterface *canvasWidget);

	canvas::CanvasModel *canvasModel() const { return m_canvasModel; }
	void setCanvasModel(canvas::CanvasModel *canvasModel);

	const QColor &clearColor() const { return m_clearColor; }
	const QSize &canvasSize() const { return m_canvasSize; }
	const QRect &canvasViewTileArea() const { return m_canvasViewTileArea; }
	const QTransform &transform() const { return m_transform; }
	const QTransform &invertedTransform() const { return m_invertedTransform; }

	bool shouldRenderSmooth() const;
	bool isTabletEnabled() const { return m_tabletEnabled; }
	bool isTouchDrawEnabled() const;
	bool isTouchPanEnabled() const;
	bool isTouchDrawOrPanEnabled() const;

	KisCubicCurve pressureCurve() const { return m_pressureCurve; }

	void setCanvasVisible(bool canvasVisible);
	bool isCanvasVisible() const { return m_canvasVisible; }

	qreal zoom() const { return m_zoom; }
	qreal actualZoom() const { return actualZoomFor(m_zoom); }
	qreal actualZoomFor(qreal zoom) const { return zoom / devicePixelRatioF(); }
	qreal rotation() const;
	qreal pixelGridScale() const { return m_pixelGridScale; }

	void scrollTo(const QPointF &pos);
	void scrollBy(int x, int y);
	void scrollByF(qreal x, qreal y);
	void setZoom(qreal zoom);
	void setZoomAt(qreal zoom, const QPointF &pos);
	void resetZoomCenter();
	void resetZoomCursor();
	void zoomTo(const QRect &rect, int steps);
	void zoomToFit();
	void zoomToFitHeight();
	void zoomToFitWidth();
	void zoomInCenter();
	void zoomInCursor();
	void zoomOutCenter();
	void zoomOutCursor();
	void zoomSteps(int steps);
	void zoomStepsAt(int steps, const QPointF &point);
	void setRotation(qreal degrees);
	void resetRotation();
	void resetRotationCursor();
	void rotateStepClockwise();
	void rotateStepClockwiseCursor();
	void rotateStepCounterClockwise();
	void rotateStepCounterClockwiseCursor();
	void setFlip(bool flip);
	void setMirror(bool mirror);

	QPoint viewCenterPoint() const;
	bool isPointVisible(const QPointF &point) const;
	QRectF screenRect() const;

	void updateViewSize();
	void updateCanvasSize(int width, int height, int offsetX, int offsetY);
	void withTileCache(const std::function<void(canvas::TileCache &)> &fn);

	void handleEnter();
	void handleLeave();
	void handleFocusIn();
	void clearKeys();

	void handleMouseMove(QMouseEvent *event);
	void handleMousePress(QMouseEvent *event);
	void handleMouseRelease(QMouseEvent *event);

	void handleTabletMove(QTabletEvent *event);
	void handleTabletPress(QTabletEvent *event);
	void handleTabletRelease(QTabletEvent *event);

	void handleTouchBegin(QTouchEvent *event);
	void handleTouchUpdate(QTouchEvent *event);
	void handleTouchEnd(QTouchEvent *event, bool cancel);

	void handleGesture(QGestureEvent *event);

	void handleWheel(QWheelEvent *event);

	void handleKeyPress(QKeyEvent *event);
	void handleKeyRelease(QKeyEvent *event);

	void setOutlineSize(int outlineSize);
	void setOutlineMode(bool subpixel, bool square, bool force);
	void setPixelGrid(bool pixelGrid);
	void setPointerTracking(bool pointerTracking);
	void setToolCapabilities(
		bool allowColorPick, bool allowToolAdjust, bool toolHandlesRightClick,
		bool fractionalTool);
	void setToolCursor(const QCursor &toolCursor);
	void setBrushBlendMode(int brushBlendMode);

#ifdef __EMSCRIPTEN__
	void setEnableEraserOverride(bool enableEraserOverride);
#endif

	bool isOutlineVisible() const;
	bool isOutlineVisibleInMode() const { return m_outlineVisibleInMode; }
	int outlineSize() const { return m_outlineSize; }
	qreal outlineWidth() const { return m_outlineWidth; }
	QPointF outlinePos() const;
	bool isSquareOutline() const { return m_squareOutline; }

	void setLockReasons(QFlags<view::Lock::Reason> reasons);
	void setLockDescription(const QString &lockDescription);
	void setToolState(int toolState);
	void setSaveInProgress(bool saveInProgress);

signals:
	void clearColorChanged();
	void renderSmoothChanged();
	void canvasOffset(int xOffset, int yOffset);
	void canvasVisibleChanged();
	void transformChanged(qreal zoom, qreal angle);
	void viewChanged(const QPolygonF &view);
	void pixelGridScaleChanged();
	void scrollAreaChanged(
		int minH, int maxH, int valueH, int pageStepH, int singleStepH,
		int minV, int maxV, int valueV, int pageStepV, int singleStepV);
	void tileCacheDirtyCheckNeeded();
	void outlineChanged();
	void transformNoticeChanged();
	void lockNoticeChanged();
	void pointerMove(const QPointF &point);
	void penDown(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool right, qreal angle, qreal zoom,
		bool mirror, bool flip, bool constrain, bool center,
		const QPointF &viewPos, int deviceType, bool eraserOverride);
	void penMove(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool constrain, bool center,
		const QPointF &viewPos);
	void penHover(
		const QPointF &point, qreal angle, qreal zoom, bool mirror, bool flip);
	void penUp(bool constrain, bool center);
	void coordinatesChanged(const QPointF &coordinates);
	void quickAdjust(qreal value);
	void cursorChanged(const QCursor &cursor);
	void toggleActionActivated(int action);
	void touchTapActionActivated(int action);
	void saveInProgressChanged(bool saveInProgress);

private:
	static constexpr qreal SCROLL_STEP = 64.0;
	static constexpr qreal ROTATION_STEP = 5.0;
	static constexpr qreal DISCRETE_ROTATION_STEP = 15.0;
	static constexpr int ZOOM_TO_MINIMUM_DIMENSION = 15;

	enum class PenMode { Normal, Colorpick, Layerpick };
	enum class PenState { Up, MouseDown, TabletDown };
	enum class ViewDragMode { None, Prepared, Started };

	void setClearColor(const QColor clearColor);
	void setRenderSmooth(bool renderSmooth);
	void setTabletEnabled(bool tabletEnabled);
	void setSerializedPressureCurve(const QString &serializedPressureCurve);
	void setOutlineWidth(qreal outlineWidth);
	void setCanvasShortcuts(QVariantMap canvasShortcuts);
	void setShowTransformNotices(bool showTransformNotices);
	void setTabletEventTimerDelay(int tabletEventTimerDelay);
	void startTabletEventTimer();

	void penMoveEvent(
		long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, Qt::KeyboardModifiers modifiers);

	void penPressEvent(
		long long timeMsec, const QPointF &posf, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers, int deviceType, bool eraserOverride);

	void penReleaseEvent(
		long long timeMsec, const QPointF &posf, Qt::MouseButton button,
		Qt::KeyboardModifiers modifiers);

	void
	touchPressEvent(QEvent *event, long long timeMsec, const QPointF &posf);
	void touchMoveEvent(long long timeMsec, const QPointF &posf);
	void touchReleaseEvent(long long timeMsec, const QPointF &posf);
	void touchZoomRotate(qreal zoom, qreal rotation);

	void moveDrag(const QPoint &point);
	void pickColor(const QPointF &point, const QPointF &posf);
	void touchColorPick(const QPointF &posf);
	void finishTouchColorPick();

	void resetCursor();
	void updateOutlinePos(QPointF point);
	void setOutlinePos(QPointF point);
	void updateOutline();
	void updateOutlineVisibleInMode();
	QPointF getOutlinePos() const;
	qreal getOutlineWidth() const;
	void changeOutline(const std::function<void()> &block);

	static QCursor generateDotCursor();
	void setViewportCursor(const QCursor &cursor);
	int getCurrentCursorStyle() const;
	void setBrushCursorStyle(int brushCursorStyle);
	void setEraseCursorStyle(int eraseCursorStyle);
	void setAlphaLockCursorStyle(int alphaLockCursorStyle);

	QPointF mousePosF(QMouseEvent *event) const;
	QPointF tabletPosF(QTabletEvent *event) const;
	QPointF wheelPosF(QWheelEvent *event) const;
	static bool isSynthetic(QMouseEvent *event);
	static bool isSyntheticTouch(QMouseEvent *event);

	Qt::KeyboardModifiers getKeyboardModifiers(const QKeyEvent *event) const;
	Qt::KeyboardModifiers getMouseModifiers(const QMouseEvent *event) const;
	Qt::KeyboardModifiers getTabletModifiers(const QTabletEvent *event) const;
	Qt::KeyboardModifiers getWheelModifiers(const QWheelEvent *event) const;
	Qt::KeyboardModifiers getFallbackModifiers() const;

	canvas::Point mapPenPointToCanvasF(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation) const;

	QPointF mapPointFromCanvasF(const QPointF &point) const;
	QPolygonF mapRectFromCanvas(const QRect &rect) const;
	QPointF mapPointToCanvas(const QPoint &point) const;
	QPointF mapPointToCanvasF(const QPointF &point) const;
	QPolygonF mapRectToCanvasF(const QRectF &rect) const;
	QPoint mapPointFromGlobal(const QPoint &point) const;

	void updateCanvasTransform(const std::function<void()> &block);
	void resetCanvasTransform();
	void updatePixelGridScale();
	void updateSceneTransform();
	void updatePosBounds();
	void clampPos();

	QTransform calculateCanvasTransform() const
	{
		return calculateCanvasTransformFrom(
			m_pos, m_zoom, m_rotation, m_mirror, m_flip);
	}

	QTransform calculateCanvasTransformFrom(
		const QPointF &pos, qreal zoom, qreal rotation, bool mirror,
		bool flip) const;

	static void mirrorFlip(QTransform &matrix, bool mirror, bool flip);

	bool isRotationInverted() const { return m_mirror ^ m_flip; }

	void setZoomToFit(Qt::Orientations orientations);
	void setRotationSnap(qreal degrees);
	void rotateByDiscreteSteps(int steps);

	void emitTransformChanged();
	void emitViewRectChanged();
	void emitScrollAreaChanged();

	QRect canvasRect() const;
	QRectF canvasRectF() const;
	QPointF viewCenterF() const;
	QRectF viewRectF() const;
	QSize viewSize() const;
	QSizeF viewSizeF() const;
	qreal devicePixelRatioF() const;
	QPointF viewToCanvasOffset() const;
	QPointF viewTransformOffset() const;
	void translateByViewTransformOffset(QTransform &prev, QTransform &cur);
	QPointF cursorPosOrCenter() const;

	QString getZoomNoticeText() const;
	QString getRotationNoticeText() const;
	void showTouchTransformNotice();
	void showTransformNotice(const QString &text);
	void updateLockNotice();

	CanvasScene *m_scene;
	CanvasInterface *m_canvasWidget = nullptr;
	canvas::CanvasModel *m_canvasModel = nullptr;
	QColor m_clearColor;
	bool m_renderSmooth = false;
	bool m_tabletEnabled = true;
	KisCubicCurve m_pressureCurve;
	bool m_pixelGrid = true;
	bool m_pointerTracking = false;

	QPointF m_pos;
	qreal m_zoom = 1.0;
	qreal m_rotation = 0.0;
	bool m_flip = false;
	bool m_mirror = false;
	QRectF m_posBounds;
	QTransform m_transform;
	QTransform m_invertedTransform;
	qreal m_pixelGridScale = 0.0;

	QSize m_canvasSize;
	QRect m_canvasViewTileArea;
	bool m_canvasVisible = true;

	bool m_showOutline = true;
	bool m_subpixelOutline = true;
	bool m_squareOutline = false;
	bool m_forceOutline = false;
	bool m_outlineVisibleInMode = true;
	int m_outlineSize = 0;
	qreal m_outlineWidth = 1.0;
	QPointF m_outlinePos;

	QCursor m_dotCursor;
	QCursor m_triangleRightCursor;
	QCursor m_triangleLeftCursor;
	QCursor m_eraserCursor;
	QCursor m_colorPickCursor;
	QCursor m_layerPickCursor;
	QCursor m_zoomCursor;
	QCursor m_rotateCursor;
	QCursor m_rotateDiscreteCursor;
	QCursor m_checkCursor;
	QCursor m_toolCursor;
	QCursor m_currentCursor;

	int m_brushCursorStyle;
	int m_eraseCursorStyle;
	int m_alphaLockCursorStyle;
	int m_brushBlendMode;

	bool m_allowColorPick = false;
	bool m_allowToolAdjust = false;
	bool m_toolHandlesRightClick = false;
	bool m_fractionalTool = false;

#ifdef __EMSCRIPTEN__
	bool m_enableEraserOverride = false;
#endif

	CanvasShortcuts m_canvasShortcuts;
	QSet<Qt::Key> m_keysDown;
	PenMode m_penMode = PenMode::Normal;
	PenState m_penState = PenState::Up;
	QDeadlineTimer m_tabletEventTimer;
	int m_tabletEventTimerDelay = 0;
	TouchHandler *m_touch;

	canvas::Point m_prevPoint;
	qreal m_pointerDistance = 0.0;
	qreal m_pointerVelocity = 0.0;

	ViewDragMode m_dragMode = ViewDragMode::None;
	CanvasShortcuts::Action m_dragAction;
	Qt::MouseButton m_dragButton;
	Qt::KeyboardModifiers m_dragModifiers;
	bool m_dragInverted = false;
	bool m_dragSwapAxes = false;
	QPoint m_dragLastPoint;
	QPointF m_dragCanvasPoint;
	qreal m_dragDiscreteRotation = 0.0;
	qreal m_dragSnapRotation = 0.0;
	int m_zoomWheelDelta = 0;

	bool m_pickingColor = false;
	bool m_canvasSizeChanging = false;
	bool m_blockNotices = false;
	bool m_showTransformNotices = false;
	int m_toolState;
	bool m_locked = false;
	QString m_lockDescription;
	bool m_saveInProgress = false;

	bool m_hoveringOverHud = false;
};

}

#endif
