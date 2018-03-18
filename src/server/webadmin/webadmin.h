/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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
#ifndef WEBADMIN_H
#define WEBADMIN_H

#include <QObject>

class MicroHttpd;

namespace server {

class MultiServer;

class Webadmin : public QObject
{
	Q_OBJECT
public:
	Webadmin(QObject *parent=0);

	/**
	 * @brief Enable basic authentication
	 *
	 * This should be called before the server is started
	 *
	 * @param userpass username & password (separated by ':')
	 */
	void setBasicAuth(const QString &userpass);

	//! Set the session server to administrate
	void setSessions(MultiServer *server);

	/**
	 * @brief Set the subnet from which the web admin can be accessed.
	 *
	 * By default, only localhost is accepted
	 *
	 * @param subnet
	 * @return false if subnet is invalid
	 */
	bool setAccessSubnet(const QString &subnet);

	/**
	 * @brief Start the admin web server
	 *
	 */
	void start(quint16 port);
	void startFd(int fd);

	/**
	 * @brief Get libmicrohttpd's version number
	 */
	static QString version();

private:
	MicroHttpd *m_server;
};

}

#endif // WEBADMIN_H
