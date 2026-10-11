// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_ZOOM_H
#define LIBCLIENT_TOOLS_ZOOM_H
#include "libclient/tools/clickdetector.h"
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
	void modify(const ModifyParams &params) override;
	void end(const EndParams &params) override;

	void setScrub(bool scrub);

private:
	static constexpr int STEPS = 3;
	static constexpr int MIN_DIMENSION = 15;

	void updatePreview(bool center) const;
	void removePreview() const;
	QRect getRect(bool center) const;

	bool m_scrub = false;
	bool m_drawing = false;
	bool m_reverse = false;
	bool m_scrubbing = false;
	QPoint m_start;
	QPoint m_end;
	QPointF m_scrubPos;
	QPointF m_scrubLast;
	ClickDetector m_clickDetector;
};

}

#endif
