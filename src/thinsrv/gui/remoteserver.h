// SPDX-License-Identifier: GPL-3.0-or-later

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
