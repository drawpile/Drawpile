/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2021 Calle Laakkonen

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

#include "acl.h"

#include <QHash>

namespace canvas {

struct AclState::Data {
	DP_UserAcls users;
	DP_FeatureTiers features;
	QHash<int, Layer> layers;
	uint8_t localUser;

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

#ifdef QT_DEBUG
	char *dump = acls.dump();
	qDebug("Acls %s:%s%s%s %s", reset ? "reset" : "changed", users ? " users" : "",
		layers ? " layers" : "", features ? " features" : "", dump);
	DP_free(dump);
#endif

	if(users || reset) {
		updateUserBits(acls, reset);
	}

	if(layers || reset) {
		updateLayers(acls, reset);
	}

	if(features || reset) {
		updateFeatures(acls, reset);
	}
}

void AclState::updateUserBits(const drawdance::AclState &acls, bool reset)
{
	const bool wasOp = amOperator();
	const bool wasLocked = amLocked();
	const auto hadFeatures = featureFlags(d->features, d->tier());

	d->users = acls.users();

	const bool amOpNow = amOperator();
	const bool amLockedNow = amLocked();

	emit userBitsChanged(this);

	if(wasOp != amOpNow || reset) {
		emit localOpChanged(amOpNow);
	}

	if(wasLocked != amLockedNow || reset) {
		emit localLockChanged(amLockedNow);
	}

	emitFeatureChanges(hadFeatures, featureFlags(d->features, d->tier()), reset);
}

void AclState::updateFeatures(const drawdance::AclState &acls, bool reset)
{
	int hadFeatures = featureFlags(d->features, d->tier());
	d->features = acls.featureTiers();
	emit featureTiersChanged(d->features);
	emitFeatureChanges(hadFeatures, featureFlags(d->features, d->tier()), reset);
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
	return d->users.all_locked || isLocked(d->localUser);
}

bool AclState::isSessionLocked() const
{
	return d->users.all_locked;
}

bool AclState::isLayerLocked(uint16_t layerId) const
{
	if(!d->layers.contains(layerId))
		return false;
	const Layer &l = d->layers[layerId];
	return l.locked || (!l.exclusive.isEmpty() && !l.exclusive.contains(d->localUser));
}

bool AclState::canUseFeature(DP_Feature feature) const
{
	const DP_AccessTier t = d->tier();
	for (int i = 0; i < DP_FEATURE_COUNT; ++i) {
		if (feature == i) {
			return t <= d->features.tiers[i];
		}
	}
	return false;
}

uint8_t AclState::localUserId() const
{
	return d->localUser;
}

DP_FeatureTiers AclState::featureTiers() const
{
	return d->features;
}

AclState::Layer AclState::layerAcl(uint16_t layerId) const
{
	return d->layers[layerId];
}

}
