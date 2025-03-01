// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

bool Tool::isActiveTool() const
{
	return m_owner.activeTool() == m_type;
}

void Tool::setCapability(Capability capability, bool enabled)
{
	if(m_capabilities.testFlag(capability) != enabled) {
		m_capabilities.setFlag(capability, enabled);
		if(isActiveTool()) {
			emit m_owner.toolCapabilitiesChanged(m_capabilities);
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
