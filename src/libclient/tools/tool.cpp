// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/tool.h"
#include "libclient/net/client.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

void Tool::setHandlesRightClick(bool handlesRightClick)
{
	if(handlesRightClick != m_handlesRightClick) {
		m_handlesRightClick = handlesRightClick;
		emit m_owner.toolCapabilitiesChanged(
			m_allowColorPick, m_allowToolAdjust, m_handlesRightClick,
			m_fractional, m_ignoresSelections);
	}
}

void Tool::setCursor(const QCursor &cursor)
{
	if(m_cursor != cursor) {
		m_cursor = cursor;
		emit m_owner.toolCursorChanged(cursor);
	}
}

bool Tool::shouldDisguiseSelectionsAsPutImage() const
{
	// If we're connected to a thick server, we don't want to send it unknown
	// message types because that'll get us kicked.
	return m_owner.client()->seemsConnectedToThickServer();
}

}
