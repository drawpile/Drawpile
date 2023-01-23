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

#include "thinsrv/gui/userlistmodel.h"

#include <QJsonObject>

namespace server {
namespace gui {

UserListModel::UserListModel(QObject *parent)
	: JsonListModel(
	{
		{"session", tr("Session")},
		{"id", tr("ID")},
		{"name", tr("Name")},
		{"ip", tr("Address")},
		{"features", tr("Features")}
	}, parent)
{
}

QVariant UserListModel::getData(const QString &key, const QJsonObject &u) const
{
	if(key == "features") {
		QStringList f;
		if(u["op"].toBool())
			f << "OP";
		if(u["mod"].toBool())
			f << "MOD";
		if(u["auth"].toBool())
			f << "Signed in";
		if(u["secure"].toBool())
			f << "TLS";
		return f.join(", ");

	} else {
		return u[key];
	}
}

Qt::ItemFlags UserListModel::getFlags(const QJsonObject &obj) const
{
	if(obj["online"].toBool(true))
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	else
		return Qt::ItemIsSelectable;
}

}
}
