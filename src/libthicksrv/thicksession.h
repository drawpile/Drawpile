/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef DP_SERVER_THICKSESSION_H
#define DP_SERVER_THICKSESSION_H

#include "../libserver/session.h"

namespace canvas {
	class AclFilter;
	class StateTracker;
}

namespace server {

/**
 * The (thick) serverside session state.
 */
class ThickSession : public Session {
	Q_OBJECT
public:
	/**
	 * Construct a fully self contained ThickSession
	 */
	ThickSession(ServerConfig *config, sessionlisting::Announcements *announcements, const QString &id, const QString &idAlias, const QString &founder, QObject *parent=nullptr);

	void readyToAutoReset(int ctxId) override;

	bool supportsAutoReset() const override { return false; }

protected:
	ThickSession(ServerConfig *config, sessionlisting::Announcements *announcements, canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, const QString &id, const QString &idAlias, const QString &founder, QObject *parent=nullptr);

	void addToHistory(protocol::MessagePtr msg) override;
    void onSessionReset() override;
	void onClientJoin(Client *client, bool host) override;

	void internalReset();

	canvas::StateTracker *stateTracker() { return m_statetracker; }

private:
	canvas::StateTracker *m_statetracker;
	canvas::AclFilter *m_aclfilter;

	protocol::MessageList m_resetImage;
	uint m_resetImageSize = 0;

	QString m_pinnedMessage;
	int m_defaultLayer = 0;
};

}

#endif

