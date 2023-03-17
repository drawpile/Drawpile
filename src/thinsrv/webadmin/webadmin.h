/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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
class QDir;

namespace server {

class MultiServer;

class Webadmin final : public QObject
{
	Q_OBJECT
public:
	Webadmin(QObject *parent=nullptr);

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
	 * @brief Set static file directory
	 *
	 * If set, files from this directory will be served at the root path.
	 * Requests for any URL ending with / will return first the index file of that
	 * directory, if present, then the root index file.
	 */
	void setStaticFileRoot(const QDir &path);

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
	 * @brief Restart the server
	 */
	Q_SLOT void restart();

	/**
	 * @brief Get libmicrohttpd's version number
	 */
	static QString version();

private:
	MicroHttpd *m_server;
	int m_port;
	enum { NOTSTARTED, PORT, FD } m_mode;
};

}

#endif // WEBADMIN_H
