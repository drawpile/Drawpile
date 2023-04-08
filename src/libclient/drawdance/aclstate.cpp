// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/drawdance/aclstate.h"

namespace drawdance {

AclState::AclState()
    : m_data(DP_acl_state_new())
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

void AclState::reset(uint8_t localUserId)
{
    DP_acl_state_reset(m_data, localUserId);
}

char *AclState::dump() const
{
    return DP_acl_state_dump(m_data);
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

void AclState::toResetImage(MessageList &msgs, uint8_t userId) const
{
    DP_acl_state_reset_image_build(m_data, userId, pushMessage, &msgs);
}

void AclState::onLayerAcl(void *user, int layerId, const DP_LayerAcl *layerAcl)
{
    (*static_cast<AclState::EachLayerFn *>(user))(layerId, layerAcl);
}

void AclState::pushMessage(void *user, DP_Message *msg)
{
    static_cast<MessageList *>(user)->append(drawdance::Message::noinc(msg));
}

}
