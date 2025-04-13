// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FREEHAND_H
#define TOOLS_FREEHAND_H
#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/tool.h"
#include <QTimer>

struct DP_Semaphore;

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

	void setBrushSizeLimit(int limit) override;

	void setSelectionMaskingEnabled(bool selectionMaskingEnabled) override;

private:
	void pollControl(bool enable);
	void poll();
	DP_CanvasState *sync();
	void syncUnlock();
	static void syncUnlockCallback(void *user);

	QTimer m_pollTimer;
	drawdance::BrushEngine m_brushEngine;
	DP_Semaphore *m_sem;
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
