// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/drawdance/aclstate.h"

namespace drawdance {

AclState::AclState()
	: AclState{DP_acl_state_new()}
{
}

AclState::~AclState()
{
	DP_acl_state_free(m_data);
}

DP_AclState *AclState::get()
{
	return m_data;
}

AclState AclState::clone(uint8_t localUserId) const
{
	return AclState{DP_acl_state_new_clone(m_data, localUserId)};
}

void AclState::reset(uint8_t localUserId)
{
	DP_acl_state_reset(m_data, localUserId);
}

char *AclState::dump() const
{
	return DP_acl_state_dump(m_data);
}

uint8_t AclState::localUserId() const
{
	return DP_acl_state_local_user_id(m_data);
}

DP_UserAcls AclState::users() const
{
	return DP_acl_state_users(m_data);
}

DP_FeatureTiers AclState::featureTiers() const
{
	return DP_acl_state_feature_tiers(m_data);
}

void AclState::eachLayerAcl(AclState::EachLayerFn fn) const
{
	DP_acl_state_layers_each(m_data, &AclState::onLayerAcl, &fn);
}

uint8_t AclState::handle(const net::Message &msg, bool overrideAcls)
{
	return DP_acl_state_handle(m_data, msg.get(), overrideAcls);
}

void AclState::toResetImage(
	net::MessageList &msgs, uint8_t userId, unsigned int includeFlags,
	const QHash<int, int> *overrideTiers) const
{
	ResetImageParams params = {msgs, overrideTiers};
	DP_acl_state_reset_image_build(
		m_data, userId, includeFlags,
		overrideTiers ? overrideFeatureTier : nullptr, pushMessage, &params);
}

AclState::AclState(DP_AclState *data)
	: m_data{data}
{
}

void AclState::onLayerAcl(void *user, int layerId, const DP_LayerAcl *layerAcl)
{
	(*static_cast<AclState::EachLayerFn *>(user))(layerId, layerAcl);
}

DP_AccessTier AclState::overrideFeatureTier(
	void *user, DP_Feature feature, DP_AccessTier originalTier)
{
	const QHash<int, int> &overrideTiers =
		*static_cast<ResetImageParams *>(user)->overrideTiers;
	QHash<int, int>::const_iterator found =
		overrideTiers.constFind(int(feature));
	if(found != overrideTiers.constEnd()) {
		int tier = found.value();
		if(tier >= 0 && tier < DP_ACCESS_TIER_COUNT) {
			return DP_AccessTier(tier);
		}
	}
	return originalTier;
}

bool AclState::pushMessage(void *user, DP_Message *msg)
{
	static_cast<ResetImageParams *>(user)->msgs.append(
		net::Message::noinc(msg));
	return true;
}

}
