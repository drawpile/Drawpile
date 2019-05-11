/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#ifndef DRAWPILE_NET_UPNP_H
#define DRAWPILE_NET_UPNP_H

#include <QObject>

class UPnPClient : public QObject {
	Q_OBJECT
public:
	explicit UPnPClient(QObject *parent=nullptr);
	~UPnPClient();

	void activateForward(int port);
	void deactivateForward(int port);
	void fetchExternalIp();

	/**
	 * @brief Get a shared instance of the UPnP client
	 *
	 * Note! The client lives on a separate thread!
	 *
	 * @return client instance
	 */
	static UPnPClient *instance();

signals:
	void externalIp(const QString &ip);

private:
	void doDiscover();
	Q_INVOKABLE void doActivateForward(int port);
	Q_INVOKABLE void doDeactivateForward(int port);
	Q_INVOKABLE void doFetchExternalIp();


	struct Private;
	Private *d;
};

#endif
