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

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

private:
	void updatePreview() const;
	void removePreview() const;
	QRect getRect() const;

	QPoint m_start, m_end;
	bool m_reverse = false;
	bool m_zooming = false;
};

}

#endif
