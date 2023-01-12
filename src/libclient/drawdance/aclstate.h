#ifndef DRAWDANCE_ACLSTATE_H
#define DRAWDANCE_ACLSTATE_H

extern "C" {
#include <dpmsg/acl.h>
}

#include <functional>
#include <QtGlobal>

namespace drawdance {

class AclState {
    using EachLayerFn = std::function<void (int, const DP_LayerAcl *)>;

public:
    AclState();
    ~AclState();

    AclState(const AclState &) = delete;
    AclState(AclState &&) = delete;
    AclState &operator=(const AclState &) = delete;
    AclState &operator=(AclState &&) = delete;

    DP_AclState *get();

    void reset(uint8_t localUserId);

    char *dump() const;

    DP_UserAcls users() const;

    DP_FeatureTiers featureTiers() const;

    void eachLayerAcl(EachLayerFn fn) const;

private:
    static void onLayerAcl(void *user, int layerId, const DP_LayerAcl *layerAcl);

    DP_AclState *m_data;
};

}

#endif
