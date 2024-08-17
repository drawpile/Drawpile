// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_SERVER_H
#define DP_NET_SERVER_H
#include "libshared/net/messagequeue.h"
#include <QList>
#include <QObject>
#include <QSslError>

class QSslCertificate;
class QUrl;

namespace net {

class Client;
class LoginHandler;
class Message;
class MessageQueue;

/**
 * \brief Abstract base class for servers interfaces
 */
class Server : public QObject {
	Q_OBJECT
	friend class LoginHandler;

public:
	enum Security {
		NO_SECURITY, // No secure connection
		NEW_HOST,	 // Secure connection to a host we haven't seen before
		KNOWN_HOST,	 // Secure connection whose certificate we have seen before
		TRUSTED_HOST // A host we have explicitly marked as trusted
	};

	static Server *
	make(const QUrl &url, int timeoutSecs, int proxyMode, Client *client);

	static QString addSchemeToUserSuppliedAddress(const QString &remoteAddress);
	static QUrl fixUpAddress(const QUrl &originalUrl, bool join);
	static QString extractAutoJoinId(const QString &path);

	explicit Server(Client *client);

	/**
	 * \brief Send a message to the server
	 */
	void sendMessage(const net::Message &msg);

	/**
	 * \brief Send multiple messages to the server
	 */
	void sendMessages(int count, const net::Message *msgs);

	void login(LoginHandler *login);
	void logout();

	/**
	 * @brief Is the user in a session
	 */
	bool isLoggedIn() const { return m_loginstate == nullptr; }

	virtual bool isWebSocket() const = 0;

	/**
	 * @brief Return the number of bytes in the upload buffer
	 */
	int uploadQueueBytes() const;

	virtual bool hasSslSupport() const = 0;

	/**
	 * @brief Current security level
	 */
	Security securityLevel() const { return m_securityLevel; }

	/**
	 * @brief Get the server's SSL certificate (if any)
	 */
	virtual QSslCertificate hostCertificate() const = 0;

	bool isSelfSignedCertificate() const { return m_selfSignedCertificate; }

	/**
	 * @brief Does the server support persistent sessions?
	 */
	bool supportsPersistence() const { return m_supportsPersistence; }
	bool supportsCryptBanImpEx() const { return m_supportsCryptBanImpEx; }
	bool supportsModBanImpEx() const { return m_supportsModBanImpEx; }
	bool supportsAbuseReports() const { return m_supportsAbuseReports; }

	void setSmoothEnabled(bool smoothEnabled)
	{
		messageQueue()->setSmoothEnabled(smoothEnabled);
	}

	void setSmoothDrainRate(int smoothDrainRate)
	{
		messageQueue()->setSmoothDrainRate(smoothDrainRate);
	}

	// Artificial lag for debugging.
	int artificialLagMs() const { return messageQueue()->artificalLagMs(); }

	void setArtificialLagMs(int msecs)
	{
		messageQueue()->setArtificialLagMs(msecs);
	}

	// Simulate network error by just closing the connection.
	void artificialDisconnect() { abortConnection(); }

signals:
	void initiatingConnection(const QUrl &url);

	void loggedIn(
		const QUrl &url, uint8_t userid, bool join, bool auth,
		const QStringList &userFlags, bool hasAutoreset, bool compatibilityMode,
		const QString &joinPassword, const QString &authId);

	void loggingOut();

	void gracefullyDisconnecting(
		MessageQueue::GracefulDisconnect, const QString &message);

	void serverDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);

	void bytesReceived(int);
	void bytesSent(int);
	void lagMeasured(qint64 lag);

protected slots:
	void handleDisconnect();
	void handleSocketStateChange(QAbstractSocket::SocketState state);
	void handleSocketError();
	void handleReadError();
	void handleWriteError();
	void handleTimeout(qint64 idleTimeout);

protected:
	// Must be called at the end of the constructor.
	void connectMessageQueue(MessageQueue *mq);
	virtual MessageQueue *messageQueue() const = 0;
	virtual void connectToHost(const QUrl &url) = 0;
	virtual void disconnectFromHost() = 0;
	virtual void abortConnection() = 0;
	virtual bool isConnected() const = 0;
	virtual QAbstractSocket::SocketError socketError() const = 0;
	virtual QString socketErrorString() const = 0;
	virtual bool loginStartTls(LoginHandler *loginstate) = 0;
	virtual bool loginIgnoreTlsErrors(const QList<QSslError> &ignore) = 0;

private slots:
	void handleMessage();
	void handleBadData(int len, int type, int contextId);

private:
	void loginSuccess();
	void loginFailure(const QString &message, const QString &errorcode);

	QString socketErrorStringWithCode() const;

	Client *const m_client;
	net::MessageList m_receiveBuffer;
	LoginHandler *m_loginstate = nullptr;
	QString m_error, m_errorcode;
	Security m_securityLevel = NO_SECURITY;
	bool m_selfSignedCertificate = false;
	bool m_localDisconnect = false;
	bool m_supportsPersistence = false;
	bool m_supportsCryptBanImpEx = false;
	bool m_supportsModBanImpEx = false;
	bool m_supportsAbuseReports = false;
	bool m_compatibilityMode = false;
};


}

#endif
