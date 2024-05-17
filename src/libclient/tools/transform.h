// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_TRANSFORM_H
#define LIBCLIENT_TOOLS_TRANSFORM_H
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/utils/transformquad.h"
#include <QCursor>
#include <QPixmap>
#include <QVector>

namespace canvas {
class TransformModel;
}

namespace tools {

class TransformTool final : public Tool {
public:
	static constexpr int HANDLE_SIZE = 25;
	enum class Mode { Scale, Distort };
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
	void hover(const HoverParams &params) override;
	void end() override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;

	void offsetActiveTool(int x, int y) override;

	void beginTemporaryMove(Tool::Type toolToReturnTo);
	void beginTemporaryPaste(
		Tool::Type toolToReturnTo, const QRect &srcBounds, const QImage &image);

	void setMode(Mode mode);

	void setFeatureAccess(bool canTransform) { m_canTransform = canTransform; }

	void mirror();
	void flip();
	void rotateCw();
	void rotateCcw();
	void shrinkToView(const QRectF &viewBounds);
	void stamp();

private:
	static constexpr int MAX_QUAD_STACK_DEPTH = 250;

	bool isTransformActive() const;
	canvas::TransformModel *getActiveTransformModel() const;
	void tryBeginMove();
	void tryBeginPaste(const QRect &srcBounds, const QImage &image);
	void endTransform(canvas::TransformModel *transform, bool applied);

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
		canvas::TransformModel *transform, const QPointF &point, bool constrain,
		bool center);

	void continueDragTransform(
		canvas::TransformModel *transform, const QPointF &point, bool constrain,
		bool center);

	void continueDragDistort(
		canvas::TransformModel *transform, const QPointF &point,
		bool constrain);

	void continueDragCommon(
		canvas::TransformModel *transform, const QPointF &point,
		bool constrain);

	void dragMove(
		canvas::TransformModel *transform, const QPointF &point,
		bool constrain);

	void dragRotate(
		canvas::TransformModel *transform, const QPointF &point,
		bool constrain);

	void dragShear(canvas::TransformModel *transform, const QPointF &point);

	void dragScaleEdge(
		canvas::TransformModel *transform, const QPointF &point, bool center);

	void dragScaleCorner(
		canvas::TransformModel *transform, const QPointF &point, bool constrain,
		bool center);

	void dragDistort(
		canvas::TransformModel *transform, const QPointF &point,
		bool constrain);

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

	void returnToPreviousTool();

	QCursor m_moveCursor =
		QCursor(QPixmap(QStringLiteral(":/cursors/move.png")), 12, 12);
	QCursor m_rotateCursor =
		QCursor(QPixmap(QStringLiteral(":/cursors/rotate.png")), 16, 16);
	QCursor m_shearCursorH =
		QCursor(QPixmap(QStringLiteral(":/cursors/shear-h.png")), 12, 12);
	QCursor m_shearCursorV =
		QCursor(QPixmap(QStringLiteral(":/cursors/shear-v.png")), 12, 12);
	QCursor m_shearCursorDiagB =
		QCursor(QPixmap(QStringLiteral(":/cursors/shear-bdiag.png")), 13, 13);
	QCursor m_shearCursorDiagF =
		QCursor(QPixmap(QStringLiteral(":/cursors/shear-fdiag.png")), 13, 13);
	QVector<TransformQuad> m_quadStack;
	int m_quadStackTop = -1;
	Mode m_mode = Mode::Scale;
	bool m_canTransform = true;
	bool m_swapDiagonal = false;
	qreal m_angle = 0.0;
	qreal m_zoom = 1.0;
	Handle m_hoverHandle = Handle::Invalid;
	Handle m_dragHandle = Handle::None;
	QPointF m_dragStartPoint;
	QPointF m_dragStartOffset;
	TransformQuad m_dragStartQuad;
	ClickDetector m_clickDetector;
	Handle m_doubleClickHandle = Handle::None;
	Tool::Type m_toolToReturnTo = Tool::Type::_LASTTOOL;
};

}

#endif
