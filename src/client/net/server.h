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
#ifndef DP_NET_SERVER_H
#define DP_NET_SERVER_H

#include "../shared/net/message.h"

#include <QObject>

class QSslCertificate;

namespace net {

/**
 * \brief Abstract base class for servers interfaces
 */
class Server : public QObject {
	Q_OBJECT
public:
	enum Security {
		NO_SECURITY, // No secure connection
		NEW_HOST,    // Secure connection to a host we haven't seen before
		KNOWN_HOST,  // Secure connection whose certificate we have seen before
		TRUSTED_HOST // A host we have explicitly marked as trusted
	};

	Server(bool local, QObject *parent);
	
	/**
	 * \brief Send a message to the server
	 */
	virtual void sendMessage(const protocol::MessagePtr &msg) = 0;
	virtual void sendMessages(const QList<protocol::MessagePtr> &msgs) = 0;

    /**
     * @brief Log out from the server
     */
    virtual void logout() = 0;

    /**
     * @brief Is this a local server?
     * @return true if local
     */
	bool isLocal() const { return m_local; }

	/**
	 * @brief Is the user in a session
	 */
	virtual bool isLoggedIn() const = 0;

	/**
	 * @brief Return the number of bytes in the upload buffer
	 */
	virtual int uploadQueueBytes() const = 0;

	/**
	 * @brief Current security level
	 */
	virtual Security securityLevel() const = 0;

	/**
	 * @brief Get the server's SSL certificate (if any)
	 */
	virtual QSslCertificate hostCertificate() const = 0;

	/**
	 * @brief Does the server support persistent sessions?
	 */
	virtual bool supportsPersistence() const = 0;
	virtual bool supportsAbuseReports() const = 0;

signals:
	void messageReceived(protocol::MessagePtr message);

private:
	bool m_local;
};


}

#endif

