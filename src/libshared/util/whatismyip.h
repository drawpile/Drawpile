/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
class WhatIsMyIp final : public QObject
{
	Q_OBJECT
public:
	explicit WhatIsMyIp(QObject *parent = nullptr);

	/**
	 * @brief Get the local address
	 *
	 * If a local IP query has finished already, this
	 * will return the true external address. Otherwise,
	 * it returns the best guess from the available
	 * network interfaces.
	 */
	QString myAddress() const { return m_ip.isEmpty() ? guessLocalAddress() : m_ip; }

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
	 * @brief Check if the address belongs to the Carrier Grade NAT block
	 * @param address
	 */
	static bool isCGNAddress(const QString &address);

	/**
	 * @brief Get the local address
	 *
	 * This function tries to find the address most likely reachable
	 * from the Internet.
	 * @return local host IP address in URL friendly format
	 */
	static QString guessLocalAddress();

signals:
	//! External IP lookup finished
	void myAddressIs(const QString &ip);

	//! An error occurred during external IP lookup
	void ipLookupError(const QString &message);

public slots:
	/**
	 * @brief Start the external IP query
	 *
	 * The signal myAddressIs(QString) will be emitted when the address is known.
	 */
	void discoverMyIp();

private:
	bool m_querying;
	QString m_ip;
};

#endif

