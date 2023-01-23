/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

