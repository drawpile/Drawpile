// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/server/builtinreset.h"
#include "libclient/drawdance/aclstate.h"
#include "libclient/drawdance/canvasstate.h"

namespace server {


void BuiltinReset::appendAclsToPostResetMessages(
	const drawdance::AclState &acls, uint8_t localUserId,
	bool compatibilityMode)
{
	acls.toResetImage(
		m_postResetMessages, localUserId,
		compatibilityMode ? DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_COMPAT_FLAGS
						  : DP_ACL_STATE_RESET_IMAGE_SESSION_RESET_FLAGS);
}

void BuiltinReset::generateResetImage(
	const drawdance::CanvasState &canvasState, bool compatibilityMode)
{
	canvasState.toResetImage(m_resetImage, 0, compatibilityMode);
	emit resetImageGenerated();
}

net::MessageList BuiltinReset::takeResetImage()
{
	m_resetImage.append(std::move(m_postResetMessages));
	return std::move(m_resetImage);
}

}
