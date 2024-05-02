// SPDX-License-Identifier: GPL-3.0-or-later

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

	static bool looksLikeLocalhost(const QString &host);

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

