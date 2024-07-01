// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_GRADIENT_H
#define LIBCLIENT_TOOLS_GRADIENT_H
#include "libclient/tools/tool.h"
#include <QAtomicInteger>
#include <QPointF>
#include <QVector>

namespace tools {

class GradientTool final : public Tool {
public:
	static constexpr int HANDLE_RADIUS = 10;
	enum class Type { Linear, Radial };
	enum class Spread { Pad, Repeat, Reflect };

	GradientTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void hover(const HoverParams &params) override;
	void end() override;

	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	void redoMultipart() override;
	bool isMultipart() const override;

private:
	static constexpr int MAX_POINTS_STACK_DEPTH = 250;

	class Task;
	friend Task;

	void gradientFinished(Task *task);

	void pushPoints();
	bool updateHoverIndex(const QPointF &targetPoint);
	void updateAnchorLine();

	void previewPending();

	QVector<QPointF> m_points;
	QVector<QPointF> m_originalPoints;
	QVector<QVector<QPointF>> m_pointsStack;
	QPointF m_dragStartPoint;
	qreal m_zoom = 1.0;
	qreal m_focus = 0.0;
	Type m_type = Type::Linear;
	Spread m_spread = Spread::Pad;
	int m_blendMode;
	int m_pointsStackTop = -1;
	int m_hoverIndex = -1;
	int m_dragIndex = -1;
	bool m_dragging = false;
	QAtomicInteger<unsigned int> m_currentExecutionId;
	QAtomicInteger<unsigned int> m_finishedExecutionId;
	QImage m_pendingImage;
	QPoint m_pendingPos;
};

}

#endif
