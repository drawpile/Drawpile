// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/rotation.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QCursor>
#include <QPixmap>

namespace tools {

RotationTool::RotationTool(ToolController &owner)
	: Tool(
		  owner, ROTATION, utils::Cursors::rotate(),
		  Capability::AllowColorPick | Capability::Fractional |
			  Capability::SendsNoMessages)
{
}

void RotationTool::setRotationMode(RotationMode rotationMode)
{
	if(rotationMode != m_rotationMode) {
		m_rotationMode = rotationMode;
		updateCursor();
	}
}

void RotationTool::setInvert(bool invert)
{
	m_invert = invert;
}

void RotationTool::begin(const BeginParams &params)
{
	if(!params.right) {
		m_rotating = true;
		m_lastViewPos = params.viewPos.toPoint();
		Q_EMIT m_owner.resetRotationToolRequested(int(m_rotationMode));
	}
}

void RotationTool::motion(const MotionParams &params)
{
	if(m_rotating) {
		QPoint viewPos = params.viewPos.toPoint();
		if(viewPos != m_lastViewPos) {
			Q_EMIT m_owner.moveRotationToolRequested(
				viewPos, m_lastViewPos, m_invert);
			m_lastViewPos = viewPos;
		}
	}
}

void RotationTool::end(const EndParams &)
{
	m_rotating = false;
}

void RotationTool::updateCursor()
{
	if(m_rotationMode == RotationMode::Discrete) {
		setCursor(utils::Cursors::rotateDiscrete());
	} else {
		setCursor(utils::Cursors::rotate());
	}
}

}
