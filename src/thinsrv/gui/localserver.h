// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LOCALSERVER_H
#define LOCALSERVER_H

#include "thinsrv/gui/server.h"

namespace server {

class MultiServer;

namespace gui {

/**
 * @brief Abstract base class for server connectors
 */
class LocalServer : public Server
{
	Q_OBJECT
public:
	explicit LocalServer(MultiServer *server, QObject *parent=nullptr);

	bool isLocal() const override { return true; }
	QString address() const override;
	int port() const override;

	// Local server specific:
	bool isRunning() const;
	void startServer();
	void stopServer();

	void makeApiRequest(const QString &requestId, JsonApiMethod method, const QStringList &path, const QJsonObject &request) override;

	//! Show a "really quit?" dialog and exit if yes is chosen
	void confirmQuit();

private slots:
	void onStartStop();

signals:
	//! Server just started or stopped
	void serverStateChanged(bool serverOn);

	void serverError(const QString &message);

private:
	// Note: this runs in another thread!
	MultiServer *m_server;
};

}
}

#endif
