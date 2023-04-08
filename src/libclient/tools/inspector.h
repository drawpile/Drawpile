// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_INSPECTOR_H
#define TOOLS_INSPECTOR_H

#include "libclient/tools/tool.h"

namespace tools {

/**
 * \brief Canvas inspector
 *
 * A moderation tool: show who last edited the selected tile
 */
class Inspector final : public Tool {
public:
	Inspector(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;
	void cancelMultipart() override;
};

}

#endif

