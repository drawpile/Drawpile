// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_LASER_H
#define TOOLS_LASER_H
#include "libclient/tools/tool.h"

namespace tools {

class LaserPointer final : public Tool {
public:
	LaserPointer(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	void setPersistence(int p) { m_persistence = p; }

private:
	int m_persistence;
	bool m_drawing;
};

}

#endif
