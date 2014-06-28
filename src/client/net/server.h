/*
   DrawPile - a collaborative drawing program.

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
#ifndef DP_NET_SERVER_H
#define DP_NET_SERVER_H

#include "../shared/net/message.h"

#include <QSslCertificate>

namespace net {

/**
 * \brief Abstract base class for servers
 */
class Server {
public:
	enum Security {
		NO_SECURITY, // No secure connection
		NEW_HOST,    // Secure connection to a host we haven't seen before
		KNOWN_HOST,  // Secure connection whose certificate we have seen before
		TRUSTED_HOST // A host we have explicitly marked as trusted
	};

	Server(bool local) : _local(local) {}
	virtual ~Server() {}
	
	/**
	 * \brief Send a message to the server
	 */
    virtual void sendMessage(protocol::MessagePtr msg) = 0;

    /**
     * @brief Enqueue messages needed to construct an initial state
     *
     * Unlike normal messages, the snapshot messages are kept in a separate queue
     * and are sent asynchronously so snapshot uploading won't entirely block this user.
     * @param msgs
     */
    virtual void sendSnapshotMessages(QList<protocol::MessagePtr> msgs) = 0;

    /**
     * @brief Log out from the server
     */
    virtual void logout() = 0;

	/**
	 * @brief Hold input messages until unpaused
	 * @param pause
	 */
	virtual void pauseInput(bool pause) = 0;

    /**
     * @brief Is this a local server?
     * @return true if local
     */
    bool isLocal() const { return _local; }

    virtual bool isLoggedIn() const { return false; }

    virtual int uploadQueueBytes() const { return 0; }

	virtual Security securityLevel() const { return NO_SECURITY; }

	virtual QSslCertificate hostCertificate() const { return QSslCertificate(); }

protected:
	/**
	 * @brief Signal a failed login
	 *
	 *
	 * @param message error message
	 * @param cancelled if true, the error was due to user cancellation
	 */
	virtual void loginFailure(const QString &, bool) {}
    virtual void loginSuccess() {}

private:
    bool _local;
};


}

#endif

