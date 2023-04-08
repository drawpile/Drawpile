// SPDX-License-Identifier: GPL-3.0-or-later

#include "libserver/jsonapi.h"

#include <QJsonObject>

namespace server {

JsonApiResult JsonApiErrorResult(JsonApiResult::Status status, const QString &message) {
	Q_ASSERT(status != JsonApiResult::Ok);
	QJsonObject o;
	o["status"] = "error";
	o["message"] = message;
	return JsonApiResult {
		status,
		QJsonDocument(o)
	};
}

std::tuple<QString, QStringList> popApiPath(const QStringList &path)
{
	if(path.isEmpty())
		return std::make_tuple(QString(), QStringList());
	QStringList tail = path.mid(1);
	return std::make_tuple(path.at(0), tail);
}

}

