// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_ROTATION_H
#define LIBCLIENT_TOOLS_ROTATION_H
#include "libclient/tools/enums.h"
#include "libclient/tools/tool.h"
#include <QPoint>

namespace tools {

class RotationTool final : public Tool {
public:
	RotationTool(ToolController &owner);

	void setRotationMode(RotationMode rotationMode);
	void setInvert(bool invert);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

private:
	void updateCursor();

	RotationMode m_rotationMode = RotationMode::Normal;
	QPoint m_lastViewPos;
	bool m_invert = false;
	bool m_rotating = false;
};

}

#endif
