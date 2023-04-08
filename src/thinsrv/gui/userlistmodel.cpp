// SPDX-License-Identifier: GPL-3.0-or-later

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
