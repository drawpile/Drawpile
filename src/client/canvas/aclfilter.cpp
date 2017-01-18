/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include "../shared/net/meta.h"
#include "../shared/net/meta2.h"
#include "../shared/net/pen.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"

namespace canvas {

AclFilter::AclFilter(QObject *parent)
	: QObject(parent)
{
}

void AclFilter::reset(int myId, bool localMode)
{
	m_layers.clear();
	m_myId = myId;
	m_isOperator = localMode;
	m_sessionLocked = false;
	m_localUserLocked = false;
	m_layerCtrlLocked = false;
	m_imagesLocked = false;
	m_ownLayers = false;
	m_userLayers.clear();

	m_lockDefault = false;

	m_ops.clear();
	m_userlocks.clear();

	if(localMode)
		m_ops << myId;

	emit localOpChanged(m_isOperator);
	emit localLockChanged(false);
	emit ownLayersChanged(m_ownLayers);
	emit layerControlLockChanged(m_layerCtrlLocked);
	emit imageCmdLockChanged(m_imagesLocked);
	emit lockByDefaultChanged(m_lockDefault);
}

// Get the ID of the layer's creator. This assumes the ID prefixing convention is used.
static uint8_t layerCreator(uint16_t layerId) {
	return layerId >> 8;
}

bool AclFilter::filterMessage(const protocol::Message &msg)
{
	using namespace protocol;

	// User list is empty in local mode
	const bool isOpUser = m_ops.contains(msg.contextId());

	// First: check if this is an operator-only command
	if(msg.contextId()!=0 && msg.isOpCommand() && !isOpUser)
		return false;

	// Special commands that affect access controls
	switch(msg.type()) {
	case MSG_USER_JOIN:
		// Set our own default lock. We can't set the user list item's properties
		// here, since it hasn't been created yet.
		if(msg.contextId() == m_myId && isLockedByDefault())
			setUserLock(true);

		// Make sure the user's OP status bits are up to date
		emit operatorListChanged(m_ops);
		break;

	case MSG_SESSION_OWNER:
		updateSessionOwnership(static_cast<const SessionOwner&>(msg));
		return true;
	case MSG_LAYER_ACL: {
		const auto &lmsg = static_cast<const LayerACL&>(msg);
		if(isOpUser || (isOwnLayers() && layerCreator(lmsg.id()) == msg.contextId())) {
			m_layers[lmsg.id()] = LayerAcl(lmsg.locked(), lmsg.exclusive());
			emit layerAclChange(lmsg.id(), lmsg.locked(), lmsg.exclusive());

			// Emit this to refresh the UI in case our selected layer was (un)locked.
			// (We don't actually know which layer is selected in the UI here.)
			emit localLockChanged(isLocked());
			return true;
		}
		return false;
	}
	case MSG_SESSION_ACL: {
		const auto &lmsg = static_cast<const SessionACL&>(msg);

		setSessionLock(lmsg.isSessionLocked());
		setLayerControlLock(lmsg.isLayerControlLocked());
		setOwnLayers(lmsg.isOwnLayers());
		setLockImages(lmsg.isImagesLocked());
		setLockByDefault(lmsg.isLockedByDefault());

		return true;
	}

	case MSG_USER_ACL: {
		const auto &lmsg = static_cast<const UserACL&>(msg);
		m_userlocks = lmsg.ids();
		emit userLocksChanged(lmsg.ids());
		setUserLock(lmsg.ids().contains(m_myId));
		return true;
	}
	case MSG_TOOLCHANGE:
		m_userLayers[msg.contextId()] = static_cast<const ToolChange&>(msg).layer();
		return true;
	default: break;
	}

	// General action filtering when user is locked
	if(msg.isCommand() && (m_sessionLocked || m_userlocks.contains(msg.contextId())))
		return false;

	// Message specific filtering
	switch(msg.type()) {
	case MSG_CHAT:
		// Only OPs can pin messages
		if(static_cast<const protocol::Chat&>(msg).isPin() && !isOpUser)
			return false;
		break;

	case MSG_LAYER_CREATE:
	case MSG_LAYER_ATTR:
	case MSG_LAYER_RETITLE:
	case MSG_LAYER_DELETE: {
		uint16_t layerId=0;
		if(msg.type() == MSG_LAYER_CREATE) {
			layerId = static_cast<const protocol::LayerCreate&>(msg).id();
			if(!isOpUser && (layerId>>8) != msg.contextId()) {
				qWarning("non-op user %d tried to create layer with context id %d", msg.contextId(), (layerId>>8));
				return false;
			}
		} else if(msg.type() == MSG_LAYER_ATTR) layerId = static_cast<const protocol::LayerAttributes&>(msg).id();
		else if(msg.type() == MSG_LAYER_RETITLE) layerId = static_cast<const protocol::LayerRetitle&>(msg).id();
		else if(msg.type() == MSG_LAYER_DELETE) layerId = static_cast<const protocol::LayerDelete&>(msg).id();

		// In OwnLayer mode, users may create, delete and adjust their own layers.
		// Otherwise, session operator privileges are required.
		if(isLayerControlLocked() && !isOpUser && !(isOwnLayers() && layerCreator(layerId) == msg.contextId()))
			return false;
		break;
	}

	case MSG_LAYER_ORDER:
		return isOpUser || !isLayerControlLocked();
	case MSG_PUTIMAGE:
		return !((isImagesLocked() && !isOpUser) || isLayerLockedFor(static_cast<const PutImage&>(msg).layer(), msg.contextId()));
	case MSG_FILLRECT:
		return !((isImagesLocked() && !isOpUser) || isLayerLockedFor(static_cast<const FillRect&>(msg).layer(), msg.contextId()));

	case MSG_PEN_MOVE:
		return !isLayerLockedFor(m_userLayers[msg.contextId()], msg.contextId());

	case MSG_ANNOTATION_CREATE: {
		const uint16_t annotationId = static_cast<const AnnotationCreate&>(msg).id();
		if(!isOpUser && (annotationId>>8) != msg.contextId()) {
			qWarning("non-op user %d tried to create annotation with context id %d", msg.contextId(), (annotationId>>8));
			return false;
		}
		break;
	}
	case MSG_ANNOTATION_EDIT: {
		const protocol::AnnotationEdit &ae = static_cast<const AnnotationEdit&>(msg);
		if(m_protectedAnnotations.contains(ae.id()) && !isOpUser && (ae.id()>>8)!=msg.contextId())
			return false;
		if((ae.flags() & protocol::AnnotationEdit::FLAG_PROTECT))
			m_protectedAnnotations.insert(ae.id());
		else
			m_protectedAnnotations.remove(ae.id());
		break;
	}
	case MSG_ANNOTATION_DELETE:
	case MSG_ANNOTATION_RESHAPE: {
		uint16_t id = msg.type() == MSG_ANNOTATION_DELETE ? static_cast<const AnnotationDelete&>(msg).id() : static_cast<const AnnotationReshape&>(msg).id();
		if(m_protectedAnnotations.contains(id) && !isOpUser && (id>>8)!=msg.contextId())
			return false;
		break;
	}

	default: break;
	}

	return true;
}

bool AclFilter::isLayerLockedFor(int layerId, uint8_t userId) const
{
	if(!m_layers.contains(layerId))
		return false;

	const LayerAcl &l = m_layers[layerId];
	return l.locked || l.exclusive.contains(userId);
}

void AclFilter::updateSessionOwnership(const protocol::SessionOwner &msg)
{
	m_ops = msg.ids();
	if(msg.contextId()!=0 && !m_ops.contains(msg.contextId()))
		m_ops << msg.contextId();

	emit operatorListChanged(m_ops);
	setOperator(m_ops.contains(m_myId));
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

void AclFilter::setLockImages(bool lock)
{
	if(m_imagesLocked != lock) {
		m_imagesLocked = lock;
		emit imageCmdLockChanged(lock);
	}
}

void AclFilter::setLockByDefault(bool lock)
{
	if(m_lockDefault != lock) {
		m_lockDefault = lock;
		emit lockByDefaultChanged(lock);
	}
}

void AclFilter::setOwnLayers(bool own)
{
	if(m_ownLayers != own) {
		m_ownLayers = own;
		emit ownLayersChanged(own);
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

	if(m_ownLayers)
		flags |= protocol::SessionACL::LOCK_OWNLAYERS;

	return flags;
}

bool AclFilter::canUseLayerControls(int layerId) const
{
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked() || (isOwnLayers() && layerCreator(layerId) == m_myId));
}

bool AclFilter::canCreateLayer() const
{
	return !isLocalUserLocked() && (isLocalUserOperator() || !isLayerControlLocked() || isOwnLayers());
}

}
