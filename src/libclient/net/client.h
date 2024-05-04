// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H
#include "libclient/net/server.h"
#include <QObject>
#include <QSslCertificate>
#include <QUrl>

class QJsonObject;
class QJsonArray;
class QTimer;
struct DP_MsgData;

namespace utils {
class AndroidWakeLock;
class AndroidWifiLock;
}

namespace net {

class Message;
class LoginHandler;
struct ServerReply;

/**
 * The client for accessing the drawing server.
 */
class Client final : public QObject {
	Q_OBJECT
public:
	enum UserFlag {
		None = 0x0,
		Mod = 0x1,
		WebSession = 0x2,
	};
	Q_DECLARE_FLAGS(UserFlags, UserFlag)

	explicit Client(QObject *parent = nullptr);

	/**
	 * @brief Connect to a remote server
	 * @param loginhandler the login handler to use
	 */
	void
	connectToServer(int timeoutSecs, LoginHandler *loginhandler, bool builtin);

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
	QUrl sessionUrl(bool includeUser = false) const;
	void setSessionUrl(const QUrl &url) { m_lastUrl = url; }

	const QUrl &connectionUrl() const { return m_connectionUrl; }

	/**
	 * @brief Is the client connected by network?
	 * @return true if a network connection is open
	 */
	bool isConnected() const { return m_server != nullptr; }

	bool isBuiltin() const { return m_builtin; }

	/**
	 * @brief Is the user connected and logged in?
	 * @return true if there is an active network connection and login process
	 * has completed
	 */
	bool isLoggedIn() const { return m_server && m_server->isLoggedIn(); }

	bool isWebSocket() const { return m_server && m_server->isWebSocket(); }

	/**
	 * @brief Is the user logged in as an authenticated user?
	 */
	bool isAuthenticated() const { return m_isAuthenticated; }

	/**
	 * @brief Is this user a moderator?
	 *
	 * Moderator status is a feature of the user account and cannot change
	 * during the connection.
	 */
	bool isModerator() const { return m_userFlags.testFlag(UserFlag::Mod); }

	bool canManageWebSession() const
	{
		return m_userFlags.testFlag(UserFlag::WebSession);
	}

	/**
	 * @brief Get connection security level
	 */
	Server::Security securityLevel() const
	{
		return m_server ? m_server->securityLevel()
						: Server::Security::NO_SECURITY;
	}

	/**
	 * @brief Get host certificate
	 *
	 * This is meaningful only if securityLevel != NO_SECURITY
	 */
	QSslCertificate hostCertificate() const
	{
		return m_server ? m_server->hostCertificate() : QSslCertificate();
	}

	bool isSelfSignedCertificate() const
	{
		return m_server && m_server->isSelfSignedCertificate();
	}

	/**
	 * @brief Does the server support persistent sessions?
	 *
	 * TODO for version 3.0: Change this to sessionSupportsPersistence
	 */
	bool serverSuppotsPersistence() const
	{
		return m_server && m_server->supportsPersistence();
	}

	bool serverSupportsCryptBanImpEx() const
	{
		return m_server && m_server->supportsCryptBanImpEx();
	}

	bool serverSupportsModBanImpEx() const
	{
		return m_server && m_server->supportsModBanImpEx();
	}

	/**
	 * @brief Can the server receive abuse reports?
	 */
	bool serverSupportsReports() const
	{
		return m_server && m_server->supportsAbuseReports();
	}

	bool sessionSupportsAutoReset() const { return m_supportsAutoReset; }

	bool isCompatibilityMode() const { return m_compatibilityMode; }

	/**
	 * @brief Get the number of bytes waiting to be sent
	 * @return upload queue length
	 */
	int uploadQueueBytes() const;

	//! Are we expecting more incoming data?
	bool isFullyCaughtUp() const { return m_catchupTo == 0; }

	int artificialLagMs() const
	{
		return m_server ? m_server->artificialLagMs() : 0;
	}

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
	void sendMessage(const net::Message &msg);

	/**
	 * @brief Send messages to the server
	 *
	 * A drawingCommandLocal signal will be emitted for drawing command
	 * messages.
	 */
	void sendMessages(int count, const net::Message *msgs);

	/**
	 * @brief Send a single reset message to the server
	 *
	 * Just a convenience method around sendResetMessages.
	 */
	void sendResetMessage(const net::Message &msg);

	/**
	 * @brief Send the reset image to the server
	 *
	 * The difference between this and sendEnvelope is that drawingCommandLocal
	 * will not be emitted, as the reset image was generate from content already
	 * on the canvas.
	 */
	void sendResetMessages(int count, const net::Message *msgs);

	void setSmoothDrainRate(int smoothDrainRate);

	void requestBanExport(bool plain);
	void requestBanImport(const QString &bans);
	void requestUpdateAuthList(const QJsonArray &list);

signals:
	void messagesReceived(int count, const net::Message *msgs);
	void drawingCommandsLocal(int count, const net::Message *msgs);
	void catchupProgress(int percentage);
	void bansExported(const QByteArray &bans);
	void bansImported(int total, int imported);
	void bansImpExError(const QString &message);

	void needSnapshot();
	void sessionResetted();
	void sessionConfChange(const QJsonObject &config);
	void sessionOutOfSpace();

	void serverConnected(const QString &address, int port);
	void serverLoggedIn(
		bool join, bool compatibilityMode, const QString &joinPassword,
		const QString &authId);
	void serverDisconnecting();
	void serverDisconnected(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
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
	void currentBrushRequested(int userId, const QString &correlator);
	void currentBrushReceived(int userId, const QJsonObject &info);

private slots:
	void setConnectionUrl(const QUrl &url);
	void handleMessages(int count, net::Message *msgs);
	void handleConnect(
		const QUrl &url, uint8_t userid, bool join, bool auth,
		const QStringList &userFlags, bool supportsAutoReset,
		bool compatibilityMode, const QString &joinPassword,
		const QString &authId);
	void handleDisconnect(
		const QString &message, const QString &errorcode, bool localDisconnect,
		bool anyMessageReceived);
	void nudgeCatchup();

private:
	static constexpr int CATCHUP_TIMER_MSEC = 4000;

	void sendCompatibleMessages(int count, const net::Message *msgs);
	void sendCompatibleResetMessages(int count, const net::Message *msgs);
	QVector<net::Message>
	filterCompatibleMessages(int count, const net::Message *msgs);

	void handleServerReply(const ServerReply &msg, int handledMessageIndex = 0);
	QString translateMessage(
		const QJsonObject &reply,
		const QString &fallbackKey = QStringLiteral("message"));
	void handleResetRequest(const ServerReply &msg);
	void handleData(const net::Message &msg);
	void handleUserInfo(const net::Message &msg, DP_MsgData *md);
	void finishCatchup(const char *reason, int handledMessageIndex = 0);

	Server *m_server = nullptr;
#ifdef Q_OS_ANDROID
	utils::AndroidWakeLock *m_wakeLock = nullptr;
	utils::AndroidWifiLock *m_wifiLock = nullptr;
#endif

	QUrl m_connectionUrl;
	QUrl m_lastUrl;
	uint8_t m_myId = 1;
	bool m_builtin = false;
	UserFlags m_userFlags = UserFlag::None;
	bool m_isAuthenticated = false;
	bool m_supportsAutoReset = false;
	bool m_compatibilityMode = false;

	int m_catchupTo = 0;
	int m_caughtUp = 0;
	int m_catchupProgress = 0;
	int m_catchupKey = 0;
	QTimer *m_catchupTimer;

	int m_smoothDrainRate = 0;
};

}

#endif
