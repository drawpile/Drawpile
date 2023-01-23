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
