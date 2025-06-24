// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_ACL_H
#define LIBCLIENT_CANVAS_ACL_H
#include "libclient/drawdance/aclstate.h"
#include <QObject>
#include <QVector>

namespace canvas {

/**
 * Access control list state that is relevant to the UI.
 */
class AclState final : public QObject {
	Q_OBJECT
public:
	struct Layer {
		bool contentLocked = false;
		bool propsLocked = false;
		bool moveLocked = false;
		DP_AccessTier tier = DP_ACCESS_TIER_GUEST;
		QVector<uint8_t> exclusive;

		uint8_t flags() const;
		bool operator!=(const Layer &other) const;
	};

	explicit AclState(QObject *parent = nullptr);
	~AclState() override;

	uint8_t localUserId();
	void setLocalUserId(uint8_t localUser);

	void updateUserBits(const drawdance::AclState &acls, bool reset);
	void updateFeatures(const drawdance::AclState &acls, bool reset);
	void updateLayers(const drawdance::AclState &acls, bool reset);

	//! Is this user an operator?
	bool isOperator(uint8_t userId) const;

	//! Is this user trusted?
	bool isTrusted(uint8_t userId) const;

	//! Is this user locked (via user specific lock)
	bool isLocked(uint8_t userId) const;

	//! Is the local user an operator?
	bool amOperator() const;

	//! Is the local user locked (via user specific lock or general lock)
	bool amLocked() const;

	//! Is the local user trusted?
	bool amTrusted() const;

	//! Is there a general session lock in place?
	bool isSessionLocked() const;

	//! Is the given layer locked for this user (ignoring canvaswide lock)
	bool isLayerLocked(int layerId) const;
	bool isLayerPropsLocked(int layerId) const;
	bool isLayerMoveLocked(int layerId) const;

	//! Is this feature available for us?
	bool canUseFeature(DP_Feature feature) const;

	//! Is the canvas, layer and we not locked and can we edit it?
	bool canEditLayer(int layerId) const;

	//! Do we own the given layer?
	bool isOwnLayer(int layerId) const;

	//! Get the given feature limit for us. A negative number means no limit.
	int featureLimit(DP_FeatureLimit featureLimit);

	//! Get the ID of the local user
	uint8_t localUserId() const;

	//! Get the required access tier to use this feature
	DP_FeatureTiers featureTiers() const;

	//! Get the layer's access control list
	Layer layerAcl(int layerId) const;

	bool isResetLocked() const;

	void aclsChanged(
		const drawdance::AclState &acls, int aclChangeFlags, bool reset);
	void resetLockSet(bool locked);

signals:
	//! Local user's operator status has changed
	void localOpChanged(bool op);

	//! Local user's lock status (either via user specific lock or general lock)
	//! has changed
	void localLockChanged(bool locked);

	//! The ACL for this layer has changed
	void layerAclChanged(int id);

	//! The local user's access to a feature has changed
	void featureAccessChanged(DP_Feature feature, bool canUse);

	//! User lock/op/trust bits have changed
	void userBitsChanged(const AclState *acl);

	//! Feature access tiers have changed
	void featureTiersChanged(const DP_FeatureTiers &);

	void ownFeatureLimitChanged(DP_FeatureLimit featureLimit, int value);

	void featureLimitForTierChanged(
		DP_FeatureLimit featureLimit, DP_AccessTier tier, int value);

	void resetLockChanged(bool locked);

private:
	void emitFeatureChanges(int before, int now, bool reset);
	void emitLimitChanges(const int *hadLimits, DP_AccessTier tier, bool reset);

	struct Data;
	Data *d;
};

}

#endif
