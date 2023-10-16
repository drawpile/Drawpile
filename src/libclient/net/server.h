// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_SERVER_H
#define DP_NET_SERVER_H
#include "libshared/net/protover.h"
#include <QObject>

class QSslCertificate;

namespace net {

class Message;

/**
 * \brief Abstract base class for servers interfaces
 */
class Server : public QObject {
	Q_OBJECT
public:
	enum Security {
		NO_SECURITY, // No secure connection
		NEW_HOST,	 // Secure connection to a host we haven't seen before
		KNOWN_HOST,	 // Secure connection whose certificate we have seen before
		TRUSTED_HOST // A host we have explicitly marked as trusted
	};

	Server(QObject *parent);

	/**
	 * \brief Send a message to the server
	 */
	virtual void sendMessage(const net::Message &msg) = 0;

	/**
	 * \brief Send multiple messages to the server
	 */
	virtual void sendMessages(int count, const net::Message *msgs) = 0;

	/**
	 * @brief Log out from the server
	 */
	virtual void logout() = 0;

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
	virtual bool supportsCryptBanImpEx() const = 0;
	virtual bool supportsModBanImpEx() const = 0;
	virtual bool supportsAbuseReports() const = 0;

	virtual void setSmoothEnabled(bool smoothEnabled) = 0;
	virtual void setSmoothDrainRate(int smoothDrainRate) = 0;

	// Artificial lag for debugging.
	virtual int artificialLagMs() const = 0;
	virtual void setArtificialLagMs(int msecs) = 0;

	// Simulate network error by just closing the connection.
	virtual void artificialDisconnect() = 0;

signals:
	void messagesReceived(int count, net::Message *msgs);
};


}

#endif
