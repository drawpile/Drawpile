// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_SELECTION_H
#define LIBCLIENT_TOOLS_SELECTION_H
#include "libclient/net/message.h"
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"
#include <QCursor>
#include <QPolygon>
#include <QPolygonF>
#include <QRect>
#include <QRectF>

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
	bool antiAlias() const { return m_params.antiAlias; }
	bool quickDrag() const;
	int defaultOp() const { return m_params.defaultOp; }
	const QPointF &startPoint() const { return m_startPoint; }

	void updateSelectionPreview(const QPainterPath &path) const;
	void removeSelectionPreview() const;

	virtual void beginSelection(const canvas::Point &point) = 0;
	virtual void continueSelection(const canvas::Point &point) = 0;
	virtual void offsetSelection(const QPoint &offset) = 0;
	virtual void cancelSelection() = 0;
	virtual net::MessageList endSelection(uint8_t contextId) = 0;

private:
	static constexpr qreal EDGE_SLOP = 5.0;

	void updateCursor(const QPointF &point);
	void endSelection(bool click, bool onlyMask);
	net::MessageList endDeselection(uint8_t contextId);
	bool isInsideSelection(const QPointF &point, bool *atEdge = nullptr) const;

	QCursor m_originalCursor;
	QCursor m_moveContentCursor =
		QCursor(QPixmap(QStringLiteral(":/cursors/move.png")), 12, 12);
	QCursor m_moveMaskCursor =
		QCursor(QPixmap(QStringLiteral(":/cursors/move-mask.png")), 7, 7);
	int m_op = -1;
	bool m_forceSelect = false;
	ToolController::SelectionParams m_params;
	QPointF m_startPoint;
	QPointF m_lastPoint;
	qreal m_zoom = 1.0;
	ClickDetector m_clickDetector;
};

class RectangleSelection final : public SelectionTool {
public:
	RectangleSelection(ToolController &owner);

protected:
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

protected:
	void beginSelection(const canvas::Point &point) override;
	void continueSelection(const canvas::Point &point) override;
	void offsetSelection(const QPoint &offset) override;
	void cancelSelection() override;
	net::MessageList endSelection(uint8_t contextId) override;

private:
	void updatePolygonSelectionPreview();

	QPolygon m_polygon;
	QPolygonF m_polygonF;
};

}

#endif
