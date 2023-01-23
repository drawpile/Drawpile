/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "thinsrv/gui/sessionlistmodel.h"

namespace server {
namespace gui {

SessionListModel::SessionListModel(QObject *parent)
	: JsonListModel({
		{"id", tr("ID")},
		{"alias", tr("Alias")},
		{"title", tr("Title")},
		{"users", tr("Users")},
		{"options", tr("Options")},
		{"startTime", tr("Started")},
		{"size", tr("Size")}
	}, parent)
{
}


QVariant SessionListModel::getData(const QString &key, const QJsonObject &s) const
{
	if(key == "users") {
		return QString("%1/%2").arg(s["userCount"].toInt()).arg(s["maxUserCount"].toInt());

	} else if(key=="options") {
		QStringList f;
		if(s["hasPassword"].toBool())
			f << "PASS";
		if(s["closed"].toBool())
			f << "CLOSED";
		if(s["nsfm"].toBool())
			f << "NSFM";
		if(s["persistent"].toBool())
			f << "PERSISTENT";
		return f.join(", ");

	} else if(key == "size") {
		return QString("%1 MB").arg(s["size"].toInt() / double(1024*1024),0, 'f', 2);
	}

	return s[key].toVariant();
}

}
}
