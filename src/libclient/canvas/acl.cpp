// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/ids.h>
}
#include "libclient/canvas/acl.h"
#include <QHash>

namespace canvas {

struct AclState::Data {
	DP_UserAcls users;
	DP_FeatureTiers features;
	QHash<int, Layer> layers;
	uint8_t localUser;
	bool resetLocked;

	DP_AccessTier tier() const;
};

AclState::Layer::Layer()
	: locked(false), tier(DP_ACCESS_TIER_GUEST)
{ }

bool AclState::Layer::operator!=(const Layer &other) const
{
	return locked != other.locked || tier != other.tier || exclusive != other.exclusive;
}

DP_AccessTier AclState::Data::tier() const
{
	return DP_user_acls_tier(&users, localUser);
}

static int featureFlags(const DP_FeatureTiers &features, DP_AccessTier t) {
	int f = 0;
	for (int i = 0; i < DP_FEATURE_COUNT; ++i) {
		if (t <= features.tiers[i]) {
			f |= 1 << i;
		}
	}
	return f;
}

AclState::AclState(QObject *parent)
	: QObject(parent), d(new Data)
{
	memset(&d->users, 0, sizeof(DP_UserAcls));
	memset(&d->features, 0, sizeof(DP_FeatureTiers));
	d->localUser = 0;
	d->resetLocked = false;
}

uint8_t AclState::localUserId()
{
	return d->localUser;
}

void AclState::setLocalUserId(uint8_t localUser)
{
	d->localUser = localUser;
}

AclState::~AclState()
{
	delete d;
}

void AclState::aclsChanged(const drawdance::AclState &acls, int aclChangeFlags, bool reset)
{
	bool users = aclChangeFlags & DP_ACL_STATE_CHANGE_USERS_BIT;
	bool layers = aclChangeFlags & DP_ACL_STATE_CHANGE_LAYERS_BIT;
	bool features = aclChangeFlags & DP_ACL_STATE_CHANGE_FEATURE_TIERS_BIT;
	bool limits = aclChangeFlags & DP_ACL_STATE_CHANGE_FEATURE_LIMITS_BIT;

#ifdef QT_DEBUG
	char *dump = acls.dump();
	qDebug(
		"Acls %s:%s%s%s%s %s", reset ? "reset" : "changed",
		users ? " users" : "", layers ? " layers" : "",
		features ? " features" : "", limits ? " limits" : "", dump);
	DP_free(dump);
#endif

	if(users || reset) {
		updateUserBits(acls, reset);
	}

	if(layers || reset) {
		updateLayers(acls, reset);
	}

	if(features || limits || reset) {
		updateFeatures(acls, reset);
	}
}

void AclState::resetLockSet(bool locked)
{
	qDebug("Reset lock %d", locked);
	d->resetLocked = locked;
	emit resetLockChanged(locked);
	emit localLockChanged(amLocked());
}

void AclState::updateUserBits(const drawdance::AclState &acls, bool reset)
{
	DP_AccessTier hadTier = d->tier();
	bool wasOp = amOperator();
	bool wasLocked = amLocked();
	int hadFeatures = featureFlags(d->features, hadTier);
	int hadLimits[DP_FEATURE_LIMIT_COUNT];
	for(int i = 0; i < DP_FEATURE_LIMIT_COUNT; ++i) {
		hadLimits[i] = d->features.limits[i][hadTier];
	}

	d->users = acls.users();

	DP_AccessTier tierNow = d->tier();
	bool amOpNow = amOperator();
	bool amLockedNow = amLocked();

	emit userBitsChanged(this);

	if(wasOp != amOpNow || reset) {
		emit localOpChanged(amOpNow);
	}

	if(wasLocked != amLockedNow || reset) {
		emit localLockChanged(amLockedNow);
	}

	emitFeatureChanges(hadFeatures, featureFlags(d->features, tierNow), reset);
	emitLimitChanges(hadLimits, tierNow, reset);
}

void AclState::updateFeatures(const drawdance::AclState &acls, bool reset)
{
	DP_AccessTier tier = DP_user_acls_tier(&d->users, d->localUser);
	int hadFeatures = featureFlags(d->features, tier);
	int hadLimits[DP_FEATURE_LIMIT_COUNT];
	for(int i = 0; i < DP_FEATURE_LIMIT_COUNT; ++i) {
		hadLimits[i] = d->features.limits[i][tier];
	}

	d->features = acls.featureTiers();
	emit featureTiersChanged(d->features);

	emitFeatureChanges(hadFeatures, featureFlags(d->features, tier), reset);
	emitLimitChanges(hadLimits, tier, reset);
}

void AclState::updateLayers(const drawdance::AclState &acls, bool reset)
{
	const auto oldLayers = d->layers;
	QHash<int, Layer> layers;
	acls.eachLayerAcl([&layers](int layerId, const DP_LayerAcl *acl) {
		AclState::Layer l;
		l.locked = acl->locked;
		l.tier = acl->tier;
		l.exclusive.clear();

		bool allOnes = true;
		for(unsigned int i=0;i < sizeof(DP_UserBits); ++i) {
			if(acl->exclusive[i] != 0xff) {
				allOnes = false;
				break;
			}
		}

		if(!allOnes) {
			for(int i = 1; i < 256; ++i) {
				if(DP_user_bit_get(acl->exclusive, i)) {
					l.exclusive << i;
				}
			}
		}

		layers.insert(layerId, l);
	});
	d->layers = layers;

	QHashIterator<int, Layer> i(layers);
	while(i.hasNext()) {
		i.next();
		if(reset || i.value() != oldLayers.value(i.key())) {
			emit layerAclChanged(i.key());
		}
	}

	QHashIterator<int, Layer> oldi(oldLayers);
	while(oldi.hasNext()) {
		oldi.next();
		if(!layers.contains(oldi.key()))
			emit layerAclChanged(oldi.key());
	}
}

void AclState::emitFeatureChanges(int before, int now, bool reset)
{
	if(reset) {
		for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
			emit featureAccessChanged(DP_Feature(i), now & (1 << i));
		}
	} else if(before != now) {
		const int changes = before ^ now;
		for(int i = 0; i < DP_FEATURE_COUNT; ++i) {
			if(changes & (1 << i)) {
				emit featureAccessChanged(DP_Feature(i), now & (1 << i));
			}
		}
	}
}

void AclState::emitLimitChanges(
	const int *hadLimits, DP_AccessTier tier, bool reset)
{
	for(int i = 0; i < DP_FEATURE_LIMIT_COUNT; ++i) {
		int limit = d->features.limits[i][tier];
		if(reset || limit != hadLimits[i]) {
			emit featureLimitChanged(DP_FeatureLimit(i), limit);
		}
	}
}

bool AclState::isOperator(uint8_t userId) const
{
	return DP_user_acls_is_op(&d->users, userId);
}

bool AclState::isTrusted(uint8_t userId) const
{
	return DP_user_acls_is_trusted(&d->users, userId);
}

bool AclState::isLocked(uint8_t userId) const
{
	return DP_user_acls_is_locked(&d->users, userId);
}

bool AclState::amOperator() const
{
	return isOperator(d->localUser);
}

bool AclState::amTrusted() const
{
	return isTrusted(d->localUser);
}

bool AclState::amLocked() const
{
	return d->users.all_locked || d->resetLocked || isLocked(d->localUser);
}

bool AclState::isSessionLocked() const
{
	return d->users.all_locked;
}

bool AclState::isLayerLocked(int layerId) const
{
	if(!d->layers.contains(layerId))
		return false;
	const Layer &l = d->layers[layerId];
	return l.locked || DP_user_acls_tier(&d->users, d->localUser) > l.tier ||
		(!l.exclusive.isEmpty() && !l.exclusive.contains(d->localUser));
}

bool AclState::canUseFeature(DP_Feature feature) const
{
	return feature >= 0 && feature < DP_FEATURE_COUNT &&
		   d->tier() <= d->features.tiers[feature];
}

bool AclState::canEditLayer(int layerId) const
{
	return !amLocked() && !isLayerLocked(layerId) &&
		   (canUseFeature(DP_FEATURE_EDIT_LAYERS) ||
			(canUseFeature(DP_FEATURE_OWN_LAYERS) && isOwnLayer(layerId)));
}

bool AclState::isOwnLayer(int layerId) const
{
	return DP_layer_id_owner(layerId, localUserId());
}

int AclState::featureLimit(DP_FeatureLimit featureLimit)
{
	return featureLimit >= 0 && featureLimit < DP_FEATURE_LIMIT_COUNT
			   ? d->features.limits[featureLimit][d->tier()]
			   : -1;
}

uint8_t AclState::localUserId() const
{
	return d->localUser;
}

DP_FeatureTiers AclState::featureTiers() const
{
	return d->features;
}

AclState::Layer AclState::layerAcl(int layerId) const
{
	return d->layers[layerId];
}

bool AclState::isResetLocked() const
{
	return d->resetLocked;
}

}
