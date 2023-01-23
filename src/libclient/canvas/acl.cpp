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

#include "libclient/canvas/acl.h"
#include "rustpile/rustpile.h"

#include <QHash>

namespace canvas {

struct AclState::Data {
	rustpile::UserACLs users;
	rustpile::FeatureTiers features;
	QHash<int, Layer> layers;
	uint8_t localUser;

	rustpile::Tier tier() const;
};

AclState::Layer::Layer()
	: locked(false), tier(rustpile::Tier::Guest)
{ }

bool AclState::Layer::operator!=(const Layer &other) const
{
	return locked != other.locked || tier != other.tier || exclusive != other.exclusive;
}

static inline bool isUserbit(const rustpile::UserBits &bits, uint8_t user) {
	return bits[user / 8] & (1 << user % 8);
}

rustpile::Tier AclState::Data::tier() const
{
	if(isUserbit(users.operators, localUser))
		return rustpile::Tier::Operator;
	else if(isUserbit(users.trusted, localUser))
		return rustpile::Tier::Trusted;
	else if(isUserbit(users.authenticated, localUser))
		return rustpile::Tier::Authenticated;
	else
		return rustpile::Tier::Guest;
}

static int featureFlags(const rustpile::FeatureTiers &features, rustpile::Tier t) {
	int f = 0;
	if(t <= features.put_image) f |= 1<<int(Feature::PutImage);
	if(t <= features.move_rect) f |= 1<<int(Feature::RegionMove);
	if(t <= features.resize) f |= 1<<int(Feature::Resize);
	if(t <= features.background) f |= 1<<int(Feature::Background);
	if(t <= features.edit_layers) f |= 1<<int(Feature::EditLayers);
	if(t <= features.own_layers) f |= 1<<int(Feature::OwnLayers);
	if(t <= features.create_annotation) f |= 1<<int(Feature::CreateAnnotation);
	if(t <= features.laser) f |= 1<<int(Feature::Laser);
	if(t <= features.undo) f |= 1<<int(Feature::Undo);
	if(t <= features.metadata) f |= 1<<int(Feature::Metadata);
	if(t <= features.timeline) f |= 1<<int(Feature::Timeline);

	return f;
}

AclState::AclState(QObject *parent)
	: QObject(parent), d(new Data)
{
	memset(&d->users, 0, sizeof(rustpile::UserACLs));
	memset(&d->features, 0, sizeof(rustpile::FeatureTiers));
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

void AclState::updateUserBits(const rustpile::UserACLs &acls)
{
	const bool wasOp = amOperator();
	const bool wasLocked = amLocked();
	const auto hadFeatures = featureFlags(d->features, d->tier());

	d->users = acls;

	const bool amOpNow = amOperator();
	const bool amLockedNow = amLocked();

	emit userBitsChanged(this);

	if(wasOp != amOpNow) {
		emit localOpChanged(amOpNow);
	}

	if(wasLocked != amLockedNow) {
		emit localLockChanged(amLockedNow);
	}

	emitFeatureChanges(hadFeatures, featureFlags(d->features, d->tier()));
}

void AclState::updateFeatures(const rustpile::FeatureTiers &newFeatures)
{
	const auto hadFeatures = featureFlags(d->features, d->tier());
	d->features = newFeatures;

	emit featureTiersChanged(newFeatures);
	emitFeatureChanges(hadFeatures, featureFlags(d->features, d->tier()));
}

static void layerAclVisitor(void *ctx, rustpile::LayerID id, const rustpile::LayerACL *acl)
{
	AclState::Layer l;

	l.locked = acl->locked;
	l.tier = acl->tier;
	l.exclusive.clear();

	bool allOnes = true;
	for(unsigned int i=0;i<sizeof(rustpile::UserBits);++i) {
		if(acl->exclusive[i] != 0xff) {
			allOnes = false;
			break;
		}
	}

	if(!allOnes) {
		for(int i=1;i<256;++i) {
			if(isUserbit(acl->exclusive, i))
				l.exclusive << i;
		}
	}

	static_cast<QHash<int, AclState::Layer>*>(ctx)->insert(id, l);
}

void AclState::updateLayers(rustpile::PaintEngine *pe)
{
	const auto oldLayers = d->layers;
	QHash<int, Layer> layers;
	rustpile::paintengine_get_acl_layers(pe, &layers, &layerAclVisitor);
	d->layers = layers;

	QHashIterator<int, Layer> i(layers);
	while(i.hasNext()) {
		i.next();
		if(i.value() != oldLayers.value(i.key())) {
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

void AclState::emitFeatureChanges(int before, int now)
{
	if(before != now) {
		const int changes = before ^ now;
		for(int i=0;i<FeatureCount;++i) {
			if(changes & (1<<i))
				emit featureAccessChanged(Feature(i), now & (1<<i));
		}
	}
}
bool AclState::isOperator(uint8_t userId) const
{
	return isUserbit(d->users.operators, userId);
}

bool AclState::isTrusted(uint8_t userId) const
{
	return isUserbit(d->users.trusted, userId);
}

bool AclState::isLocked(uint8_t userId) const
{
	return isUserbit(d->users.locked, userId);
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

bool AclState::isLayerLocked(uint16_t layerId) const
{
	if(!d->layers.contains(layerId))
		return false;
	const Layer &l = d->layers[layerId];
	return l.locked || (!l.exclusive.isEmpty() && !l.exclusive.contains(d->localUser));
}

bool AclState::canUseFeature(Feature f) const
{
	const rustpile::Tier t = d->tier();
	switch(f) {
	case Feature::PutImage: return t <= d->features.put_image;
	case Feature::RegionMove: return t <= d->features.move_rect;
	case Feature::Resize: return t <= d->features.resize;
	case Feature::Background: return t <= d->features.background;
	case Feature::EditLayers: return t <= d->features.edit_layers;
	case Feature::OwnLayers: return t <= d->features.own_layers;
	case Feature::CreateAnnotation: return t <= d->features.create_annotation;
	case Feature::Laser: return t <= d->features.laser;
	case Feature::Undo: return t <= d->features.undo;
	case Feature::Metadata: return t <= d->features.metadata;
	case Feature::Timeline: return t <= d->features.timeline;
	}

	return false;
}

uint8_t AclState::localUserId() const
{
	return d->localUser;
}

rustpile::FeatureTiers AclState::featureTiers() const
{
	return d->features;
}

AclState::Layer AclState::layerAcl(uint16_t layerId) const
{
	return d->layers[layerId];
}

}
