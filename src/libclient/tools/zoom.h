// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_ZOOM_H
#define TOOLS_ZOOM_H

#include "libclient/tools/tool.h"

namespace tools {

/**
 * @brief Zoom tool
 *
 * This changes the view only and has no actual drawing effect.
 */
class ZoomTool final : public Tool {
public:
	ZoomTool(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

private:
	QPoint m_start, m_end;
	bool m_reverse;
};

}

#endif

