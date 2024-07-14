// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_PAN_H
#define LIBCLIENT_TOOLS_PAN_H
#include "libclient/tools/tool.h"
#include <QPointF>

namespace tools {

class PanTool final : public Tool {
public:
	PanTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

private:
	QPointF m_lastViewPos;
	bool m_panning = false;
};

}

#endif
