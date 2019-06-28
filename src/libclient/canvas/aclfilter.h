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

#ifndef ACLFILTER_H
#define ACLFILTER_H

#include "features.h"

#include <QObject>
#include <QHash>
#include <QSet>
#include <bitset>

namespace protocol {
	class Message;
	class SessionOwner;
	class TrustedUsers;
}

namespace canvas {

class UserSet {
public:
	UserSet() = default;

	bool contains(uint8_t id) const { return m_users[id]; }
	void set(uint8_t id) { m_users[id] = true; }
	bool unset(uint8_t id) { bool wasSet = m_users[id]; m_users[id] = false; return wasSet; }
	void reset() { m_users.reset(); }

	void setFromList(const QList<uint8_t> ids) {
		m_users.reset();
		for(int i=0;i<ids.size();++i)
			m_users[ids[i]] = true;
	}

	QList<uint8_t> toList() const {
		QList<uint8_t> lst;
		for(int i=0;i<256;++i)
			if(m_users[i])
				lst << i;
		return lst;
	}

private:
	std::bitset<256> m_users;
};

class AclFilter : public QObject
{
	Q_OBJECT
public:
	struct LayerAcl {
		bool locked;              // layer is locked for all users
		Tier tier;                // layer access tier (if not exclusive)
		QList<uint8_t> exclusive; // if not empty, only these users can draw on this layer
	};

	explicit AclFilter(QObject *parent=nullptr);
	AclFilter *clone(QObject *newParent=nullptr) const;

	//! Reset all access controls
	void reset(uint8_t myId, bool localMode);

	//! Go online mode: refresh status bits
	void setOnlineMode(uint8_t myId);

	/**
	 * @brief Filter a message
	 *
	 * This will also update the filter state.
	 *
	 * @param msg the message
	 * @return false if message must be dropped
	 */
	bool filterMessage(const protocol::Message &msg);

	//! Does the local user have operator privileges?
	bool isLocalUserOperator() const { return m_isOperator; }

	//! Has the local user been tagged as trusted?
	bool isLocalUserTrusted() const { return m_isTrusted; }

	//! Is there a general session lock in place?
	bool isSessionLocked() const { return m_sessionLocked; }

	//! Has the local user been locked individually?
	bool isLocalUserLocked() const { return m_localUserLocked; }

	//! Is the local user locked for any reason?
	bool isLocked() const { return m_sessionLocked | m_localUserLocked; }

	//! Check if the local user can draw on the given layer
	bool isLayerLocked(int layerId) const {
		return isLocked() || isLayerLockedFor(layerId, m_myId, userTier(m_myId));
	}

	//! Can the local user use this feature?
	bool canUseFeature(Feature f) const { return localUserTier() <= featureTier(f); }

	//! Get the local user's feature access tier
	Tier localUserTier() const { return userTier(m_myId); }

	//! Get the given feature's access tier
	Tier featureTier(Feature f) const { Q_ASSERT(int(f)>=0 && int(f) < FeatureCount); return m_featureTiers[int(f)]; }

	/**
	 * @brief Get the access controls for an individual layer
	 * If the layer has not been configured, an unlocked set will be returned.
	 */
	LayerAcl layerAcl(int id) const;

	//! Get the list of locked users
	QList<uint8_t> lockedUsers() const { return m_userlocks.toList(); }

signals:
	void localOpChanged(bool op);
	void localLockChanged(bool lock);
	void layerAclChanged(int id);

	void userLocksChanged(const QList<uint8_t> lockedUsers);
	void operatorListChanged(const QList<uint8_t> opUsers);
	void trustedUserListChanged(const QList<uint8_t> trustedUsers);

	//! The local user's access to a feature just changed
	void featureAccessChanged(Feature feature, bool canUse);

	//! A feature's access tier was changed
	void featureTierChanged(Feature feature, Tier tier);

private:
	void setOperator(bool op);
	void setTrusted(bool trusted);
	void setSessionLock(bool lock);
	void setUserLock(bool lock);
	void setLockByDefault(bool lock);
	void setFeature(Feature feature, Tier tier);

	void updateSessionOwnership(const protocol::SessionOwner &msg);
	void updateTrustedUserList(const protocol::TrustedUsers &msg);

	bool isLayerLockedFor(int layerId, uint8_t userId, Tier userTier) const;

	Tier userTier(int id) const {
		if(id == 0) // only the server can use ID 0
			return Tier::Op;
		if(m_ops.contains(id))
			return Tier::Op;
		if(m_trusted.contains(id))
			return Tier::Trusted;
		if(m_auth.contains(id))
			return Tier::Auth;
		return Tier::Guest;
	}

	int m_myId = 1;                 // the ID of the local user

	bool m_isOperator = false;      // is the local user an operator?
	bool m_isTrusted = false;       // does the local user have trusted status?
	bool m_sessionLocked = false;   // is the session locked?
	bool m_localUserLocked = false; // is the local user locked individually?

	QHash<int,LayerAcl> m_layers;          // layer access controls
	UserSet m_ops;                         // list of operators
	UserSet m_trusted;                     // list of trusted users
	UserSet m_auth;                        // list of registered users
	UserSet m_userlocks;                   // list of individually locked users
	QSet<uint16_t> m_protectedAnnotations; // list of protected annotations
	Tier m_featureTiers[FeatureCount] = {Tier::Guest}; // feature access tiers
};

}

#endif // ACLFILTER_H
