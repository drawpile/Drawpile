// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_ACLSTATE_H
#define DRAWDANCE_ACLSTATE_H
extern "C" {
#include <dpmsg/acl.h>
}
#include "libclient/net/message.h"
#include <QtGlobal>
#include <functional>

namespace drawdance {

class AclState {
	using EachLayerFn = std::function<void(int, const DP_LayerAcl *)>;

public:
	AclState();
	~AclState();

	AclState(const AclState &) = delete;
	AclState(AclState &&) = delete;
	AclState &operator=(const AclState &) = delete;
	AclState &operator=(AclState &&) = delete;

	DP_AclState *get();

	AclState clone(uint8_t localUserId) const;

	void reset(uint8_t localUserId);

	char *dump() const;

	uint8_t localUserId() const;

	DP_UserAcls users() const;

	DP_FeatureTiers featureTiers() const;

	void eachLayerAcl(EachLayerFn fn) const;

	uint8_t handle(const net::Message &msg, bool overrideAcls = false);

	void toResetImage(
		net::MessageList &msgs, uint8_t userId,
		unsigned int includeFlags) const;

private:
	AclState(DP_AclState *data);

	static void
	onLayerAcl(void *user, int layerId, const DP_LayerAcl *layerAcl);

	static bool pushMessage(void *user, DP_Message *msg);

	DP_AclState *m_data;
};

}

#endif
