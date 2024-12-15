// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

bool Tool::isActiveTool() const
{
	return m_owner.activeTool() == m_type;
}

void Tool::setHandlesRightClick(bool handlesRightClick)
{
	if(handlesRightClick != m_handlesRightClick) {
		m_handlesRightClick = handlesRightClick;
		if(isActiveTool()) {
			emit m_owner.toolCapabilitiesChanged(
				m_allowColorPick, m_allowToolAdjust, m_handlesRightClick,
				m_fractional, m_supportsPressure, m_ignoresSelections);
		}
	}
}

void Tool::setCursor(const QCursor &cursor)
{
	if(m_cursor != cursor) {
		m_cursor = cursor;
		if(isActiveTool()) {
			emit m_owner.toolCursorChanged(cursor);
		}
	}
}

void Tool::requestToolNotice(const QString &text)
{
	if(isActiveTool()) {
		emit m_owner.toolNoticeRequested(text);
	}
}

}
