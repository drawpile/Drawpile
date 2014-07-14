/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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

#ifndef WHATISMYIP_H
#define WHATISMYIP_H

#include <QObject>

/**
 * @brief A class for finding out the externally visible IP address.
 *
 * Currently ipecho.net is used to discover the address.
 */
class WhatIsMyIp : public QObject
{
	Q_OBJECT
public:
    explicit WhatIsMyIp(QObject *parent = 0);

	/**
	 * @brief Get a singleton instance of this class
	 * @return
	 */
	static WhatIsMyIp *instance();


	/**
	 * @brief Is this address one of this host's non-public addresses?
	 * @param address
	 * @return
	 */
	static bool isMyPrivateAddress(const QString &address);

	/**
	 * @brief Get the local address
	 *
	 * This function tries to find the address most likely reachable
	 * from the Internet.
	 * @return local host IP address in URL friendly format
	 */
	static QString localAddress();

signals:
	void myAddressIs(const QString &ip);

public slots:
	void discoverMyIp();

private:
	bool _querying;
	QString _ip;
};

#endif // WHATISMYIP_H
