// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/sessions.h"
#include "libserver/session.h"
#include "libserver/sessionhistory.h"

namespace server {

void Sessions::JoinResult::setInvite(
	Session *session, Client *client, const QString &inviteSecret)
{
	if(inviteSecret.isEmpty()) {
		invite = JoinResult::InviteOk;
	} else if(session) {
		switch(session->checkInvite(client, inviteSecret, false)) {
		case CheckInviteResult::InviteOk:
		case CheckInviteResult::InviteUsed:
		case CheckInviteResult::AlreadyInvited:
		case CheckInviteResult::AlreadyInvitedNameChanged:
		// No client key can happen on old clients that don't provide an
		// SID early in the login process. We let them in and check
		// again later.
		case CheckInviteResult::NoClientKey:
			invite = JoinResult::InviteOk;
			break;
		case CheckInviteResult::MaxUsesReached:
			invite = JoinResult::InviteLimitReached;
			break;
		case CheckInviteResult::NotFound:
			invite = JoinResult::InviteNotFound;
			break;
		}
	} else {
		invite = JoinResult::InviteNotFound;
	}
}

}
