// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_COLORPICKER_H
#define TOOLS_COLORPICKER_H

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

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

	//! Pick from the current active layer only?
	void setPickFromCurrentLayer(bool p) { m_pickFromCurrentLayer = p; }

	//! Set picker size (color is averaged from the area)
	void setSize(int size) { m_size = size; }

private:
	bool m_pickFromCurrentLayer;
	int m_size;
};

}

#endif

