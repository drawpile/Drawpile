/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include <QObject>
#include <QHash>

namespace protocol {
	class Message;
	class SessionOwner;
}

namespace paintcore {
	class LayerStack;
}

namespace canvas {

class UserListModel;

class AclFilter : public QObject
{
	Q_OBJECT
public:
	AclFilter(UserListModel *users, paintcore::LayerStack *layers, QObject *parent = 0);

	//! Reset all access controls
	void reset(int myId, bool localMode);

	/**
	 * @brief Filter a message
	 *
	 * This will also update the filter state.
	 *
	 * @param msg the message
	 * @return false if message must be dropped
	 */
	bool filterMessage(const protocol::Message &msg);

	bool isLocalUserOperator() const { return m_isOperator; }
	bool isSessionLocked() const { return m_sessionLocked; }
	bool isLocalUserLocked() const { return m_localUserLocked; }
	bool isLocked() const { return m_sessionLocked | m_localUserLocked; }
	bool isLockedByDefault() const { return m_lockDefault; }

	//! Can the local user access this layer's controls?
	bool canUseLayerControls(int layerId) const;

	//! Can the local user create new layers?
	bool canCreateLayer() const;

	//! Are layer controls limited to session operators
	bool isLayerControlLocked() const { return m_layerCtrlLocked; }

	/**
	 * @brief Are users allowed to control layers they've created themselves?
	 *
	 * When layer controls are locked, this allows users to adjust their own
	 * layer properties, but not anyone elses.
	 * This also allows users to set the access controls for their own layers.
	 */
	bool isOwnLayers() const { return m_ownLayers; }

	uint16_t sessionAclFlags() const;

signals:
	void localOpChanged(bool op);
	bool localLockChanged(bool lock);
	bool layerControlLockChanged(bool lock);
	void ownLayersChanged(bool own);

private:
	void setOperator(bool op);
	void setSessionLock(bool lock);
	void setUserLock(bool lock);
	void setLayerControlLock(bool lock);
	void setOwnLayers(bool own);

	void updateSessionOwnership(const protocol::SessionOwner &msg);

	bool isLayerLockedFor(int layerId, uint8_t userId) const;

	UserListModel *m_users;
	paintcore::LayerStack *m_layers;
	int m_myId;

	bool m_isOperator;
	bool m_sessionLocked;
	bool m_localUserLocked;
	bool m_layerCtrlLocked;
	bool m_ownLayers;
	bool m_lockDefault;
	QHash<int, int> m_userLayers;
};

}

#endif // ACLFILTER_H
