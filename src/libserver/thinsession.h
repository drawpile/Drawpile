/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2019 Calle Laakkonen

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

#ifndef DP_SERVER_THINSESSION_H
#define DP_SERVER_THINSESSION_H

#include "libserver/session.h"

namespace server {

/**
 * The (thin) serverside session state.
 */
class ThinSession final : public Session {
	Q_OBJECT
public:
	ThinSession(SessionHistory *history, ServerConfig *config, sessionlisting::Announcements *announcements, QObject *parent=nullptr);

	void readyToAutoReset(int ctxId) override;

	void cleanupHistoryCache();

	bool supportsAutoReset() const override { return true; }

protected:
	void addToHistory(protocol::MessagePtr msg) override;
	void onSessionReset() override;
	void onClientJoin(Client *client, bool host) override;

private:
	enum class AutoResetState { NotSent, Queried, Requested};

	QElapsedTimer m_lastStatusUpdate;

	AutoResetState m_autoResetRequestStatus = AutoResetState::NotSent;
};

}

#endif

