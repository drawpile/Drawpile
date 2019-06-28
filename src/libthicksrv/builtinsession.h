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

#ifndef BUILTINSESSION_H
#define BUILTINSESSION_H

#include "thicksession.h"

namespace server {

/**
 * @brief A specialized ThickSession that piggybacks on the client's canvas
 */
class BuiltinSession : public ThickSession
{
	Q_OBJECT
public:
	BuiltinSession(ServerConfig *config, sessionlisting::Announcements *announcements, canvas::StateTracker *statetracker, const canvas::AclFilter *aclFilter, const QUuid &id, const QString &idAlias, const QString &founder, QObject *parent=nullptr);

public slots:
	void doInternalResetNow();

protected:
	void onClientJoin(Client *client, bool host) override;

private:
	bool m_softResetRequested = false;
};

}

#endif // BUILTINSESSION_H
