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

#ifndef REMOTESERVER_H
#define REMOTESERVER_H

#include "thinsrv/gui/server.h"

#include <QUrl>

namespace server {
namespace gui {

class RemoteServer final : public Server
{
	Q_OBJECT
public:
	explicit RemoteServer(const QUrl &url, QObject *parent=nullptr);

	bool isLocal() const override { return false; }
	QString address() const override { return m_baseurl.host(); }
	int port() const override { return 27750; /* todo */ }

	void makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request) override;

private:
	QUrl m_baseurl;
};

}
}

#endif
