// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

void Tool::setHandlesRightClick(bool handlesRightClick)
{
	if(handlesRightClick != m_handlesRightClick) {
		m_handlesRightClick = handlesRightClick;
		emit m_owner.toolCapabilitiesChanged(
			m_allowColorPick, m_allowToolAdjust, m_handlesRightClick);
	}
}

}
