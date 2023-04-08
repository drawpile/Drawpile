// SPDX-License-Identifier: GPL-3.0-or-later

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
