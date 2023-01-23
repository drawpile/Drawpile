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

#include "thinsrv/gui/server.h"
#include "thinsrv/gui/sessionlistmodel.h"

#include <QJsonObject>
#include <QDebug>

namespace server {
namespace gui {

static const QString REFRESH_REQID = "sessionlist";

Server::Server(QObject *parent)
	: QObject(parent), m_sessions(new SessionListModel(this))
{

}

void Server::refreshSessionList()
{
	makeApiRequest(REFRESH_REQID, JsonApiMethod::Get, QStringList() << "sessions", QJsonObject());
}

void Server::onApiResponse(const QString &requestId, const JsonApiResult &result)
{
	if(result.status != result.Ok) {
		qWarning() << requestId << "query failed:" << result.body;
	}

	if(requestId == REFRESH_REQID) {
		QJsonArray sessions = result.body.array();
		m_sessions->setList(sessions);
		emit sessionListRefreshed(sessions);
	} else {
		emit apiResponse(requestId, result);
	}
}

}
}
