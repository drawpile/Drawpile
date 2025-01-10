// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_BEZIERTOOL_H
#define LIBCLIENT_TOOLS_BEZIERTOOL_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/tool.h"

namespace tools {

/**
 * \brief A bezier curve tool
 */
class BezierTool final : public Tool {
public:
	BezierTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void hover(const HoverParams &params) override;
	void end(const EndParams &params) override;
	void finishMultipart() override;
	void cancelMultipart() override;
	void undoMultipart() override;
	bool isMultipart() const override { return !m_points.isEmpty(); }
	bool usesBrushColor() const override { return true; }

private:
	void updatePreview();
	canvas::PointVector calculateBezierCurve() const;
	static qreal cubicBezierDistance(const QPointF points[4]);
	static QPointF cubicBezierPoint(const QPointF points[4], float t);

	struct ControlPoint {
		QPointF point;
		QPointF cp; // second control point, relative to the main point
	};

	drawdance::BrushEngine m_brushEngine;
	QVector<ControlPoint> m_points;
	QPointF m_beginPoint;
	bool m_rightButton;
	qreal m_zoom;
};

}

#endif
