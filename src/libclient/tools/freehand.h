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

	void begin(const canvas::Point &point, bool right, float zoom) override;

	void
	motion(const canvas::Point &point, bool constrain, bool center) override;

	void end() override;

	bool allowSmoothing() const override { return true; }

	void offsetActiveTool(int x, int y) override;

private:
	void pollControl(bool enable);
	void poll();

	QTimer m_pollTimer;
	drawdance::BrushEngine m_brushEngine;
	bool m_drawing;
	bool m_firstPoint;
	canvas::Point m_start;
	float m_zoom;
};

}

#endif
