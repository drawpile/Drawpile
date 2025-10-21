// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_SELECTION_H
#define LIBCLIENT_TOOLS_SELECTION_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/net/message.h"
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"
#include <QPolygon>
#include <QPolygonF>
#include <QRect>
#include <QRectF>
#include <QTimer>

class QPainterPath;

namespace canvas {
class SelectionModel;
}

namespace tools {

class SelectionTool : public Tool {
public:
	SelectionTool(ToolController &owner, Type type, QCursor cursor);

	void begin(const BeginParams &params) final override;
	void motion(const MotionParams &params) final override;
	void modify(const ModifyParams &params) final override;
	void hover(const HoverParams &params) final override;
	void end(const EndParams &params) override;

	void finishMultipart() override final;
	void cancelMultipart() override final;
	void undoMultipart() override final;
	bool isMultipart() const override final;

	void offsetActiveTool(int x, int y) override;

	static int resolveOp(bool constrain, bool center, int defaultOp);

	void setForceSelect(bool forceSelect) { m_forceSelect = forceSelect; }

protected:
	int op() const { return m_op; }
	bool antiAlias() const { return m_owner.selectionParams().antiAlias; }
	bool quickDrag() const;
	int defaultOp() const { return m_owner.selectionParams().defaultOp; }
	const QPointF &startPoint() const { return m_startPoint; }

	void updateSelectionPreview(const QPainterPath &path) const;
	void removeSelectionPreview() const;

	virtual const QCursor &getCursor(int effectiveOp) const = 0;
	virtual void beginSelection(const canvas::Point &point) = 0;
	virtual void continueSelection(const canvas::Point &point) = 0;
	virtual void offsetSelection(const QPoint &offset) = 0;
	virtual void cancelSelection() = 0;
	virtual net::MessageList endSelection(uint8_t contextId) = 0;

private:
	static constexpr qreal EDGE_SLOP = 5.0;

	void updateCursor(const QPointF &point, bool constrain, bool center);
	void endSelection(bool click, bool onlyMask);
	net::MessageList endDeselection(uint8_t contextId);
	bool isInsideSelection(const QPointF &point, bool *atEdge = nullptr) const;

	int m_op = -1;
	bool m_forceSelect = false;
	bool m_wasForceSelect = false;
	QPointF m_startPoint;
	QPointF m_lastPoint;
	qreal m_zoom = 1.0;
	ClickDetector m_clickDetector;
};

class RectangleSelection final : public SelectionTool {
public:
	RectangleSelection(ToolController &owner);

protected:
	virtual const QCursor &getCursor(int effectiveOp) const override;
	void beginSelection(const canvas::Point &point) override;
	void continueSelection(const canvas::Point &point) override;
	void offsetSelection(const QPoint &offset) override;
	void cancelSelection() override;
	net::MessageList endSelection(uint8_t contextId) override;

private:
	void updateRectangleSelectionPreview();
	QRect getRect() const;
	QRectF getRectF() const;

	QPointF m_end;
};

class PolygonSelection final : public SelectionTool {
public:
	PolygonSelection(ToolController &owner);

	void setStabilizationParams(
		int stabilizationMode, int stabilizerSampleCount, int smoothing);

protected:
	virtual const QCursor &getCursor(int effectiveOp) const override;
	void beginSelection(const canvas::Point &point) override;
	void continueSelection(const canvas::Point &point) override;
	void offsetSelection(const QPoint &offset) override;
	void cancelSelection() override;
	net::MessageList endSelection(uint8_t contextId) override;

private:
	int getEffectiveStabilizerSampleCount() const;
	int getEffectiveSmoothing() const;

	void addPoint(const QPointF &point);
	void pollControl(bool enable);
	void poll();
	void updatePolygonSelectionPreview();

	QTimer m_pollTimer;
	drawdance::StrokeEngine m_strokeEngine;
	long long m_lastTimeMsec = 0LL;
	QPolygon m_polygon;
	QPolygonF m_polygonF;
	int m_stabilizationMode = 0;
	int m_stabilizerSampleCount = 0;
	int m_smoothing = 0;
};

}

#endif
