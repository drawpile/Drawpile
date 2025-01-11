// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FREEHAND_H
#define TOOLS_FREEHAND_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/tool.h"
#include <QTimer>

namespace tools {

//! Freehand brush tool
class Freehand final : public Tool {
public:
	Freehand(ToolController &owner, bool isEraser);
	~Freehand() override;

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	bool usesBrushColor() const override { return true; }

	void offsetActiveTool(int x, int y) override;

private:
	void pollControl(bool enable);
	void poll();

	QTimer m_pollTimer;
	drawdance::BrushEngine m_brushEngine;
	bool m_drawing = false;
	bool m_firstPoint = false;
	bool m_mirror = false;
	bool m_flip = false;
	canvas::Point m_start;
	qreal m_zoom = 1.0;
	qreal m_angle = 0.0;
};

}

#endif
