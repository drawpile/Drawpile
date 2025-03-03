// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_TRANSFORM_H
#define LIBCLIENT_TOOLS_TRANSFORM_H
#include "libclient/net/message.h"
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/utils/transformquad.h"
#include <QCursor>
#include <QPixmap>
#include <QVector>

namespace canvas {
class TransformModel;
}

namespace net {
class Client;
}

namespace tools {

class TransformTool final : public Tool {
public:
	static constexpr int HANDLE_SIZE = 25;
	enum class Mode { Scale, Distort, Move };
	enum class Handle {
		Invalid = -1,
		None = 0,
		TopLeft,
		Top,
		TopRight,
		Right,
		BottomRight,
		Bottom,
		BottomLeft,
		Left,
		TopEdge,
		RightEdge,
		BottomEdge,
		LeftEdge,
		Outside,
		Inside,
	};

	TransformTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void modify(const ModifyParams &params) override;
	void hover(const HoverParams &params) override;
	void end(const EndParams &params) override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;

	void offsetActiveTool(int x, int y) override;

	void beginTemporaryMove(
		Tool::Type toolToReturnTo, bool onlyMask, bool quickMove,
		const QCursor &outsideMoveCursor);
	void beginTemporaryPaste(
		Tool::Type toolToReturnTo, const QRect &srcBounds, const QImage &image);
	void clearTemporary();

	bool handleDeselect(bool finish);

	Mode mode() const { return m_mode; }
	void setMode(Mode mode);

	bool constrain() const { return m_constrain; }
	void setConstrain(bool constrain) { m_constrain = constrain; }

	bool center() const { return m_center; }
	void setCenter(bool center) { m_center = center; }

	void setFeatureAccess(bool canTransform) { m_canTransform = canTransform; }
	void setForceMove(bool forceMove) { m_forceMove = forceMove; }
	void setApplyOnEnd(bool applyOnEnd) { m_applyOnEnd = applyOnEnd; }

	void mirror();
	void flip();
	void rotateCw();
	void rotateCcw();
	void shrinkToView(const QRectF &viewBounds);
	void stamp();

private:
	static constexpr int MAX_QUAD_STACK_DEPTH = 250;

	Mode effectiveMode() const;

	bool isTransformActive() const;
	canvas::TransformModel *getActiveTransformModel() const;
	canvas::TransformModel *
	tryBeginMove(bool firstClick, bool onlyMask, Mode mode);
	void tryBeginPaste(const QRect &srcBounds, const QImage &image);
	void endTransform(canvas::TransformModel *transform, bool applied);

	bool checkAndSend(net::Client *client, const net::MessageList &msgs);

	void updateHoverHandle(
		const canvas::TransformModel *transform, const QPointF &point);

	void
	emitStateChange(const canvas::TransformModel *transform, Handle handle);

	QCursor getHandleCursor(
		const canvas::TransformModel *transform, Handle handle) const;

	QCursor getTransformHandleCursor(
		const canvas::TransformModel *transform, Handle handle) const;

	QCursor getDistortHandleCursor(
		const canvas::TransformModel *transform, Handle handle) const;

	QCursor getMoveHandleCursor(Handle handle) const;

	QCursor getCommonHandleCursor(
		const canvas::TransformModel *transform, Handle handle) const;

	QCursor getArrowCursor(qreal angle) const;

	QCursor getShearCursor(qreal angle) const;

	QCursor getAngleCursor(
		qreal angle, const QCursor &cursorH, const QCursor &cursorV,
		const QCursor &cursorDiagB, const QCursor &cursorDiagF) const;

	const TransformQuad &
	getCursorQuad(const canvas::TransformModel *transform) const;

	bool isDragging() const { return m_dragHandle != Handle::None; }

	void
	startDrag(const canvas::TransformModel *transform, const QPointF &point);

	void continueDrag(
		canvas::TransformModel *transform, bool constrain, bool center);

	void continueDragTransform(
		canvas::TransformModel *transform, bool constrain, bool center);

	void continueDragDistort(canvas::TransformModel *transform, bool constrain);

	void continueDragCommon(canvas::TransformModel *transform, bool constrain);

	void dragMove(canvas::TransformModel *transform, bool constrain);

	void dragRotate(canvas::TransformModel *transform, bool constrain);

	void dragShear(canvas::TransformModel *transform);

	void dragScaleEdge(canvas::TransformModel *transform, bool center);

	void dragScaleCorner(
		canvas::TransformModel *transform, bool constrain, bool center);

	void dragDistort(canvas::TransformModel *transform, bool constrain);

	QPointF getConstrainedEdgeDelta(
		const QPointF &prevPoint, const QPointF &nextPoint,
		const QPointF &delta) const;

	static QPointF getPointAlongAxis(const QLineF &axis, const QPointF &target);

	static QPolygonF rotateQuad(const TransformQuad &quad, qreal angle);

	static QPolygonF rotateQuadAround(
		const TransformQuad &quad, qreal angle, const QPointF &center);

	void updateQuad(canvas::TransformModel *transform, const QPolygonF &target);

	void pushQuad(canvas::TransformModel *transform);

	Handle getHandleAt(
		const canvas::TransformModel *transform, const QPointF &point,
		qreal zoom, QPointF *outOffset = nullptr) const;

	void checkHandleAt(
		const QPointF &point, qreal zoom, Handle &inOutClosestHandle,
		qreal &inOutClosestDistance, QPointF &outOffset,
		Handle consideredHandle, const QPointF &consideredPoint) const;

	void checkHandleEdgeAt(
		const QPointF &point, qreal zoom, Handle &inOutClosestHandle,
		qreal &inOutClosestDistance, QPointF &outOffset,
		Handle consideredHandle, const QLineF &consideredEdge) const;

	static QPointF getQuadHandlePoint(
		const TransformQuad &quad, Handle handle, const QPointF &targetPoint);

	void hotSwapToSelection(Tool::Type tool);
	void returnToPreviousTool();

	QCursor m_outsideMoveCursor = Qt::ArrowCursor;
	QVector<TransformQuad> m_quadStack;
	int m_quadStackTop = -1;
	Mode m_mode = Mode::Scale;
	bool m_invertMode = false;
	bool m_canTransform = true;
	bool m_swapDiagonal = false;
	bool m_firstClick = false;
	bool m_forceMove = false;
	bool m_applyOnEnd = false;
	bool m_constrain = false;
	bool m_center = false;
	qreal m_angle = 0.0;
	qreal m_zoom = 1.0;
	Handle m_hoverHandle = Handle::Invalid;
	Handle m_dragHandle = Handle::None;
	QPointF m_dragStartPoint;
	QPointF m_dragCurrentPoint;
	QPointF m_dragStartHandlePoint;
	QPointF m_dragStartOffset;
	TransformQuad m_dragStartQuad;
	ClickDetector m_clickDetector;
	Handle m_doubleClickHandle = Handle::None;
	Tool::Type m_toolToReturnTo = Tool::Type::_LASTTOOL;
};

}

#endif
