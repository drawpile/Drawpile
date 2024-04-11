// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_PAN_H
#define LIBCLIENT_TOOLS_PAN_H
#include "libclient/tools/tool.h"
#include <QPointF>

namespace tools {

class PanTool final : public Tool {
public:
	PanTool(ToolController &owner);

	void begin(
		const canvas::Point &point, bool right, float zoom,
		const QPointF &viewPos) override;

	void motion(
		const canvas::Point &point, bool constrain, bool center,
		const QPointF &viewPos) override;

	void end() override;

private:
	QPointF m_lastViewPos;
	bool m_panning = false;
};

}

#endif
