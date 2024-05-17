// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_COLORPICKER_H
#define LIBCLIENT_TOOLS_COLORPICKER_H
#include "libclient/tools/tool.h"

namespace tools {

/**
 * \brief Color picker
 *
 * Color picker is a local tool, it does not affect the drawing board.
 */
class ColorPicker final : public Tool {
public:
	ColorPicker(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end() override;

	//! Pick from the current active layer only?
	void setPickFromCurrentLayer(bool p) { m_pickFromCurrentLayer = p; }

	//! Set picker size (color is averaged from the area)
	void setSize(int size) { m_size = size; }

private:
	void pick(const QPointF &point) const;

	bool m_picking = false;
	bool m_pickFromCurrentLayer = false;
	int m_size = 1;
};

}

#endif
