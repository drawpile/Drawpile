/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "aclfilter.h"
#include "userlist.h"
#include "layerlist.h"

#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/pen.h"
#include "../shared/net/image.h"

namespace net {

AclFilter::AclFilter(UserListModel *users, LayerListModel *layers, QObject *parent)
	: QObject(parent), m_users(users), m_layers(layers)
{
}

void AclFilter::reset(int myId, bool localMode)
{
	m_layers->unlockAll();
	m_myId = myId;
	setOperator(localMode);

	m_sessionLocked = false;
	m_localUserLocked = false;
	m_userLayers.clear();
	emit localLockChanged(false);

	setLayerControlLock(false);
	m_lockDefault = false;
}

bool AclFilter::filterMessage(const protocol::Message &msg)
{
	using namespace protocol;

	User u = m_users->getUserById(msg.contextId());

	// User list is empty in local mode
	bool isOperator = u.isOperator || (msg.contextId() == m_myId && m_isOperator);

	// First: check if this is an operator-only command
	if(msg.contextId()!=0 && msg.isOpCommand() && !isOperator)
		return false;

	// Special commands that affect access controls
	switch(msg.type()) {
	case MSG_USER_JOIN:
		// Set our own default lock. We can't set the user list item's properties
		// here, since it hasn't been created yet.
		if(msg.contextId() == m_myId && isLockedByDefault())
			setUserLock(true);
		break;

	case MSG_SESSION_OWNER:
		updateSessionOwnership(static_cast<const SessionOwner&>(msg));
		return true;
	case MSG_LAYER_ACL: {
		const auto &lmsg = static_cast<const LayerACL&>(msg);
		// TODO allow layer ACL to be used by non-operators when OwnLayers mode is active

		m_layers->updateLayerAcl(lmsg.id(), lmsg.locked(), lmsg.exclusive());

		// Emit this to refresh the UI in case our selected layer was (un)locked.
		// (We don't actually know which layer is selected in the UI here.)
		emit localLockChanged(isLocked());

		return true;
	}
	case MSG_SESSION_ACL: {
		const auto &lmsg = static_cast<const SessionACL&>(msg);

		setSessionLock(lmsg.isSessionLocked());
		setLayerControlLock(lmsg.isLayerControlLocked());
		m_lockDefault = lmsg.isLockedByDefault();

		return true;
	}

	case MSG_USER_ACL: {
		const auto &lmsg = static_cast<const UserACL&>(msg);
		m_users->updateLocks(lmsg.ids());
		setUserLock(lmsg.ids().contains(m_myId));
		return true;
	}
	case MSG_TOOLCHANGE:
		m_userLayers[msg.contextId()] = static_cast<const ToolChange&>(msg).layer();
		return true;
	default: break;
	}

	// General action filtering when user is locked
	if(msg.isCommand() && (m_sessionLocked || u.isLocked))
		return false;

	// Message specific filtering
	switch(msg.type()) {
	case MSG_LAYER_CREATE:
		// TODO OwnLayers mode
	case MSG_LAYER_ATTR:
	case MSG_LAYER_RETITLE:
	case MSG_LAYER_ORDER:
	case MSG_LAYER_DELETE:
		// TODO LockLayers mode
		break;

	case MSG_PUTIMAGE:
		return !m_layers->isLayerLockedFor(static_cast<const PutImage&>(msg).layer(), msg.contextId());
	case MSG_FILLRECT:
		return !m_layers->isLayerLockedFor(static_cast<const FillRect&>(msg).layer(), msg.contextId());

	case MSG_PEN_MOVE:
		return !m_layers->isLayerLockedFor(m_userLayers[msg.contextId()], msg.contextId());

	default: break;
	}

	return true;
}

void AclFilter::updateSessionOwnership(const protocol::SessionOwner &msg)
{
	QList<uint8_t> ids = msg.ids();
	ids.append(msg.contextId());
	m_users->updateOperators(ids);

	setOperator(ids.contains(m_myId));
}

void AclFilter::setOperator(bool op)
{
	if(op != m_isOperator) {
		m_isOperator = op;
		emit localOpChanged(op);
	}
}

void AclFilter::setSessionLock(bool lock)
{
	bool wasLocked = isLocked();
	m_sessionLocked = lock;
	if(wasLocked != isLocked())
		emit localLockChanged(isLocked());
}

void AclFilter::setUserLock(bool lock)
{
	bool wasLocked = isLocked();
	m_localUserLocked = lock;
	if(wasLocked != isLocked())
		emit localLockChanged(isLocked());
}

void AclFilter::setLayerControlLock(bool lock)
{
	if(m_layerCtrlLocked != lock) {
		m_layerCtrlLocked = lock;
		emit layerControlLockChanged(lock);
	}
}

uint16_t AclFilter::sessionAclFlags() const
{
	uint16_t flags = 0;
	if(m_sessionLocked)
		flags |= protocol::SessionACL::LOCK_SESSION;

	if(m_layerCtrlLocked)
		flags |= protocol::SessionACL::LOCK_LAYERCTRL;

	if(m_lockDefault)
		flags |= protocol::SessionACL::LOCK_LAYERCTRL;

	// TODO OwnMode
	return flags;
}

bool AclFilter::canUseLayerControls(int layerId) const
{
	// TODO OwnLayer mode
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked());
}

bool AclFilter::canCreateLayer() const
{
	// TODO OwnLayer mode
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked());
}

}
