#include "aclstate.h"

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

void AclState::onLayerAcl(void *user, int layerId, const DP_LayerAcl *layerAcl)
{
    (*static_cast<AclState::EachLayerFn *>(user))(layerId, layerAcl);
}

}
