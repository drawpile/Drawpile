// SPDX-License-Identifier: GPL-3.0-or-later

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
