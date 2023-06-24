// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H

#include "libclient/net/server.h"

#include <QObject>
#include <QSslCertificate>
#include <QUrl>

class QJsonObject;
class QJsonArray;
struct DP_MsgData;

namespace drawdance {
	class Message;
}

namespace utils {
	class AndroidWakeLock;
	class AndroidWifiLock;
}

namespace net {

class LoginHandler;
struct ServerReply;

/**
 * The client for accessing the drawing server.
 */
class Client final : public QObject {
	Q_OBJECT
public:
	explicit Client(QObject *parent=nullptr);

	/**
	 * @brief Connect to a remote server
	 * @param loginhandler the login handler to use
	 */
	void connectToServer(int timeoutSecs, LoginHandler *loginhandler);

	/**
	 * @brief Disconnect from the remote server
	 */
	void disconnectFromServer();

	/**
	 * @brief Get the local user's user/context ID
	 * @return user ID
	 */
	uint8_t myId() const { return m_myId; }

	/**
	 * Return the URL of the current (or last connected) session
	 */
	QUrl sessionUrl(bool includeUser=false) const;

	/**
	 * @brief Is the client connected by network?
	 * @return true if a network connection is open
	 */
	bool isConnected() const { return m_server != nullptr; }

	/**
	 * @brief Is the user connected and logged in?
	 * @return true if there is an active network connection and login process has completed
	 */
	bool isLoggedIn() const { return m_server && m_server->isLoggedIn(); }

	/**
	 * @brief Is the user logged in as an authenticated user?
	 */
	bool isAuthenticated() const { return m_isAuthenticated; }

	/**
	 * @brief Is this user a moderator?
	 *
	 * Moderator status is a feature of the user account and cannot change during
	 * the connection.
	 */
	bool isModerator() const { return m_moderator; }

	/**
	 * @brief Get connection security level
	 */
	Server::Security securityLevel() const { return m_server ? m_server->securityLevel() : Server::Security::NO_SECURITY; }

	/**
	 * @brief Get host certificate
	 *
	 * This is meaningful only if securityLevel != NO_SECURITY
	 */
	QSslCertificate hostCertificate() const { return m_server ? m_server->hostCertificate() : QSslCertificate(); }

	/**
	 * @brief Does the server support persistent sessions?
	 *
	 * TODO for version 3.0: Change this to sessionSupportsPersistence
	 */
	bool serverSuppotsPersistence() const { return m_server && m_server->supportsPersistence(); }

	/**
	 * @brief Can the server receive abuse reports?
	 */
	bool serverSupportsReports() const { return m_server && m_server->supportsAbuseReports(); }

	bool sessionSupportsAutoReset() const { return m_supportsAutoReset; }

	bool isCompatibilityMode() const { return m_compatibilityMode; }

	/**
	 * @brief Get the number of bytes waiting to be sent
	 * @return upload queue length
	 */
	int uploadQueueBytes() const;

	//! Are we expecting more incoming data?
	bool isFullyCaughtUp() const { return m_catchupTo == 0; }

	int artificialLagMs() const { return m_server ? m_server->artificialLagMs() : 0; }

	void setArtificialLagMs(int msecs)
	{
		if(m_server) {
			m_server->setArtificialLagMs(msecs);
		}
	}

	void artificialDisconnect()
	{
		if(m_server) {
			m_server->artificialDisconnect();
		}
	}

public slots:
	/**
	 * @brief Send a single message to the server
	 *
	 * Just a convenience method around sendMessages.
	 */
	void sendMessage(const drawdance::Message &msg);

	/**
	 * @brief Send messages to the server
	 *
	 * A drawingCommandLocal signal will be emitted for drawing command messages.
	 */
	void sendMessages(int count, const drawdance::Message *msgs);

	/**
	 * @brief Send a single reset message to the server
	 *
	 * Just a convenience method around sendResetMessages.
	 */
	void sendResetMessage(const drawdance::Message &msg);

	/**
	 * @brief Send the reset image to the server
	 *
	 * The difference between this and sendEnvelope is that drawingCommandLocal
	 * will not be emitted, as the reset image was generate from content already
	 * on the canvas.
	 */
	void sendResetMessages(int count, const drawdance::Message *msgs);

	void setSmoothDrainRate(int smoothDrainRate);

signals:
	void messagesReceived(int count, const drawdance::Message *msgs);
	void drawingCommandsLocal(int count, const drawdance::Message *msgs);
	void catchupProgress(int percentage);

	void needSnapshot();
	void sessionResetted();
	void sessionConfChange(const QJsonObject &config);

	void serverConnected(const QString &address, int port);
	void serverLoggedIn(bool join, bool compatibilityMode);
	void serverDisconnecting();
	void serverDisconnected(const QString &message, const QString &errorcode, bool localDisconnect);
	void youWereKicked(const QString &kickedBy);

	void serverMessage(const QString &message, bool isAlert);
	void serverLog(const QString &message);

	void bytesReceived(int);
	void bytesSent(int);
	void lagMeasured(qint64);
	void autoresetRequested(int maxSize, bool query);
	void serverStatusUpdate(int historySize);

	void userInfoRequested(int userId);
	void userInfoReceived(int userId, const QJsonObject &info);

private slots:
	void handleMessages(int count, const drawdance::Message *msgs);
	void handleConnect(
		const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator,
		bool supportsAutoReset, bool compatibilityMode);
	void handleDisconnect(const QString &message, const QString &errorcode, bool localDisconnect);

private:
	void sendCompatibleMessages(int count, const drawdance::Message *msgs);
	void sendCompatibleResetMessages(int count, const drawdance::Message *msgs);
	QVector<drawdance::Message> filterCompatibleMessages(int count, const drawdance::Message *msgs);

	void handleServerReply(const ServerReply &msg);
	void handleResetRequest(const ServerReply &msg);
	void handleData(const drawdance::Message &msg);
	void handleUserInfo(const drawdance::Message &msg, DP_MsgData *md);

	Server *m_server = nullptr;
#ifdef Q_OS_ANDROID
	utils::AndroidWakeLock *m_wakeLock = nullptr;
	utils::AndroidWifiLock *m_wifiLock = nullptr;
#endif

	QUrl m_lastUrl;
	uint8_t m_myId = 1;
	bool m_moderator = false;
	bool m_isAuthenticated = false;
	bool m_supportsAutoReset = false;
	bool m_compatibilityMode = false;

	int m_catchupTo = 0;
	int m_caughtUp = 0;
	int m_catchupProgress = 0;

	int m_smoothDrainRate = 0;
};

}

#endif
