/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2018 Calle Laakkonen

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
#include "../shared/net/brushes.h"
#include "../shared/net/image.h"
#include "../shared/net/layer.h"
#include "../shared/net/annotation.h"
#include "../shared/net/undo.h"

namespace canvas {

AclFilter::AclFilter(QObject *parent)
	: QObject(parent)
{
}

void AclFilter::reset(int myId, bool localMode)
{
	m_layers.clear();
	m_myId = myId;
	m_isTrusted = false;
	m_sessionLocked = false;
	m_localUserLocked = false;

	m_ops.reset();
	m_trusted.reset();
	m_auth.reset();
	m_userlocks.reset();
	m_protectedAnnotations.clear();

	if(localMode)
		m_ops.set(myId);
	setOperator(localMode);

	emit localLockChanged(false);

	// Default feature access levels
	setFeature(        Feature::PutImage, Tier::Guest);
	setFeature(      Feature::RegionMove, Tier::Guest);
	setFeature(          Feature::Resize, Tier::Op);
	setFeature(      Feature::Background, Tier::Op);
	setFeature(      Feature::EditLayers, Tier::Op);
	setFeature(       Feature::OwnLayers, Tier::Guest);
	setFeature(Feature::CreateAnnotation, Tier::Guest);
	setFeature(           Feature::Laser, Tier::Guest);
	setFeature(            Feature::Undo, Tier::Guest);
	static_assert(FeatureCount == 9, "missing default feature tiers");
}

// Get the ID of the layer's creator. This assumes the ID prefixing convention is used.
static uint8_t layerCreator(uint16_t layerId) {
	return layerId >> 8;
}

bool AclFilter::filterMessage(const protocol::Message &msg)
{
	using namespace protocol;

	// Session and user specific locks apply to all Command type messages
	if(msg.isCommand() && (m_sessionLocked || m_userlocks.contains(msg.contextId())))
		return false;

	// This user's access level tier determines which features are available
	const Tier tier = userTier(msg.contextId());

	switch(msg.type()) {
	case MSG_USER_JOIN:
		if((static_cast<const UserJoin&>(msg).flags() & UserJoin::FLAG_AUTH))
			m_auth.set(msg.contextId());

		// Make sure the user's OP status bits are up to date
		emit operatorListChanged(m_ops.toList());
		break;

	case MSG_USER_LEAVE: {
		// User left: remove locks
		m_ops.unset(msg.contextId());
		m_trusted.unset(msg.contextId());
		m_auth.unset(msg.contextId());
		m_userlocks.unset(msg.contextId());

		QMutableHashIterator<int,LayerAcl> i(m_layers);
		while(i.hasNext()) {
			i.next();
			if(i.value().exclusive.removeAll(msg.contextId())>0)
				emit layerAclChanged(i.key());
		}

		// Refresh UI
		if(msg.contextId() == m_myId) {
			setOperator(false);
			setTrusted(false);
			m_localUserLocked = false;
			emit localLockChanged(isLocked());
		}
		break; }

	case MSG_SESSION_OWNER:
		// This command is validated by the server
		updateSessionOwnership(static_cast<const SessionOwner&>(msg));
		return true;

	case MSG_TRUSTED_USERS:
		// this command is validated by the server
		updateTrustedUserList(static_cast<const TrustedUsers&>(msg));
		return true;

	case MSG_LAYER_ACL:
		if( tier <= featureTier(Feature::EditLayers) ||
		   (tier <= featureTier(Feature::OwnLayers) && layerCreator(msg.layer()) == msg.contextId())
		) {
			const auto &lmsg = static_cast<const LayerACL&>(msg);

			if(lmsg.layer() == 0) {
				// Layer 0 sets the general session lock.
				// Exclusive user list is not used in this case.
				if(tier > Tier::Op)
					return false;
				setSessionLock(lmsg.locked());
				return true;
			}

			const Tier tier = Tier(qBound(0, int(lmsg.tier()), TierCount));
			m_layers[lmsg.layer()] = LayerAcl { lmsg.locked(), tier, lmsg.exclusive() };

			// Emit this to refresh the UI in case our selected layer was (un)locked.
			// (We don't actually know which layer is selected in the UI here.)
			emit localLockChanged(isLocked());
			emit layerAclChanged(lmsg.layer());

			return true;
		}
		return false;

	case MSG_FEATURE_LEVELS: {
		if(tier > Tier::Op)
			return false;

		const auto &flmsg = static_cast<const FeatureAccessLevels&>(msg);
		for(int i=0;i<canvas::FeatureCount;++i) {
			setFeature(Feature(i), Tier(qBound(0, int(flmsg.featureTier(i)), canvas::TierCount)));
		}
		return true;
	}

	case MSG_USER_ACL: {
		if(tier > Tier::Op)
			return false;

		const auto &lmsg = static_cast<const UserACL&>(msg);
		m_userlocks.setFromList(lmsg.ids());
		emit userLocksChanged(lmsg.ids());
		setUserLock(m_userlocks.contains(m_myId));
		return true;
	}

	case MSG_LAYER_DEFAULT:
		return tier == Tier::Op;

	case MSG_CHAT:
		// Only operators can pin messages
		if(static_cast<const protocol::Chat&>(msg).isPin() && tier > Tier::Op)
			return false;
		break;

	case MSG_LASERTRAIL:
		return tier <= featureTier(Feature::Laser);

	case MSG_CANVAS_RESIZE: return tier <= featureTier(Feature::Resize);
	case MSG_PUTTILE: return tier == Tier::Op;
	case MSG_CANVAS_BACKGROUND: return tier <= featureTier(Feature::Background);

	case MSG_LAYER_CREATE: {
		if(tier > Tier::Op && layerCreator(msg.layer()) != msg.contextId()) {
			qWarning("non-op user %d tried to create layer with context id %d", msg.contextId(), layerCreator(msg.layer()));
			return false;
		}

		// Must have either general or ownlayer permission to create layers
		return tier <= featureTier(Feature::EditLayers) || tier <= featureTier(Feature::OwnLayers);
	}
	case MSG_LAYER_ATTR:
		if(static_cast<const protocol::LayerAttributes&>(msg).sublayer()>0 && tier > Tier::Op) {
			// Direct sublayer editing is used only by operators during session init
			return false;
		}
		Q_FALLTHROUGH();

	case MSG_LAYER_RETITLE:
	case MSG_LAYER_DELETE: {
		const uint8_t createdBy = layerCreator(msg.layer());
		// EDITLAYERS feature gives permission to edit all layers
		// OWNLAYERS feature gives permission to edit layers created by this user
		if(
			(createdBy != msg.contextId() && tier > featureTier(Feature::EditLayers)) ||
			(createdBy == msg.contextId() && tier > featureTier(Feature::OwnLayers))
		  )
			return false;

		if(msg.type() == MSG_LAYER_DELETE)
			m_layers.remove(msg.layer());
		break;
	}
	case MSG_LAYER_ORDER:
		return tier <= featureTier(Feature::EditLayers);

	case MSG_PUTIMAGE:
	case MSG_FILLRECT:
		return tier <= featureTier(Feature::PutImage) && !isLayerLockedFor(msg.layer(), msg.contextId(), tier);

	case MSG_DRAWDABS_CLASSIC:
	case MSG_DRAWDABS_PIXEL:
		return !isLayerLockedFor(msg.layer(), msg.contextId(), tier);

	case MSG_REGION_MOVE:
		return tier <= featureTier(Feature::RegionMove) && !isLayerLockedFor(msg.layer(), msg.contextId(), tier);

	case MSG_ANNOTATION_CREATE:
		if(tier > featureTier(Feature::CreateAnnotation))
			return false;

		if(tier > Tier::Op && layerCreator(msg.layer()) != msg.contextId()) {
			qWarning("non-op user %d tried to create annotation with context id %d", msg.contextId(), layerCreator(msg.layer()));
			return false;
		}
		break;
	case MSG_ANNOTATION_EDIT:
		// Non-operators can't edit protected annotations created by other users
		if(m_protectedAnnotations.contains(layerCreator(msg.layer())) && tier > Tier::Op && layerCreator(msg.layer()) != msg.contextId())
			return false;

		if((static_cast<const AnnotationEdit&>(msg).flags() & protocol::AnnotationEdit::FLAG_PROTECT))
			m_protectedAnnotations.insert(msg.layer());
		else
			m_protectedAnnotations.remove(msg.layer());
		break;
	case MSG_ANNOTATION_DELETE:
	case MSG_ANNOTATION_RESHAPE:
		if(m_protectedAnnotations.contains(msg.layer()) && tier > Tier::Op && layerCreator(msg.layer())!=msg.contextId())
			return false;
		if(msg.type() == MSG_ANNOTATION_DELETE)
			m_protectedAnnotations.remove(msg.layer());
		break;

	case MSG_UNDO:
		if(tier > featureTier(Feature::Undo))
			return false;

		// Only operators can override Undos.
		if(tier > Tier::Op && static_cast<const Undo&>(msg).overrideId()>0)
			return false;
		break;

	default: break;
	}

	return true;
}

AclFilter::LayerAcl AclFilter::layerAcl(int layerId) const
{
	if(!m_layers.contains(layerId))
		return LayerAcl { false, Tier::Guest, QList<uint8_t>() };

	return m_layers[layerId];
}

bool AclFilter::isLayerLockedFor(int layerId, uint8_t userId, Tier userTier) const
{
	if(!m_layers.contains(layerId))
		return false;

	const LayerAcl &l = m_layers[layerId];
	// Locking a layer locks it for everyone
	if(l.locked)
		return true;

	// If the layer has not been configured for exclusive user access,
	// permit access by user tier
	if(l.exclusive.isEmpty())
		return l.tier < userTier;
	else
		return !l.exclusive.contains(userId);
}

void AclFilter::updateSessionOwnership(const protocol::SessionOwner &msg)
{
	m_ops.setFromList(msg.ids());
	emit operatorListChanged(msg.ids());
	setOperator(m_ops.contains(m_myId));
}

void AclFilter::updateTrustedUserList(const protocol::TrustedUsers &msg)
{
	m_trusted.setFromList(msg.ids());
	emit trustedUserListChanged(msg.ids());
	setTrusted(m_trusted.contains(m_myId));
}

void AclFilter::setOperator(bool op)
{
	if(op != m_isOperator) {
		m_isOperator = op;
		emit localOpChanged(op);

		// Op and Trusted status change affects available features
		for(int i=0;i<FeatureCount;++i)
			emit featureAccessChanged(Feature(i), canUseFeature(Feature(i)));
	}
}

void AclFilter::setTrusted(bool trusted)
{
	if(trusted != m_isTrusted) {
		m_isTrusted = trusted;

		// Op and Trusted status change affects available features
		for(int i=0;i<FeatureCount;++i)
			emit featureAccessChanged(Feature(i), canUseFeature(Feature(i)));
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

void AclFilter::setFeature(Feature feature, Tier tier)
{
	if(m_featureTiers[int(feature)] == tier)
		return;

	const bool hadAccess = canUseFeature(feature);
	m_featureTiers[int(feature)] = tier;

	if(canUseFeature(feature) != hadAccess)
		emit featureAccessChanged(feature, !hadAccess);

	emit featureTierChanged(feature, tier);
}

}

