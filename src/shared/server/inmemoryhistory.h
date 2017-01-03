/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef DP_SERVER_SESSION_INMEMHISTORY_H
#define DP_SERVER_SESSION_INMEMHISTORY_H

#include "sessionhistory.h"

namespace server {

/**
 * @brief A session history backend that stores the session in memory
 *
 */
class InMemoryHistory : public SessionHistory {
	Q_OBJECT
public:
	InMemoryHistory(QObject *parent=nullptr);

	std::tuple<QList<protocol::MessagePtr>, int> getBatch(int after) const override;

	void cleanupBatches(int) override { /* no caching, nothing to do */ }

protected:
	void historyAdd(const protocol::MessagePtr &msg) override;
	void historyReset(const QList<protocol::MessagePtr> &newHistory) override;

private:
	QList<protocol::MessagePtr> m_history;
};

}

#endif

