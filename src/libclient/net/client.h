/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2021 Calle Laakkonen

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
#ifndef DP_NET_CLIENT_H
#define DP_NET_CLIENT_H

#include "libclient/net/server.h"

#include <QObject>
#include <QSslCertificate>
#include <QUrl>

class QJsonObject;
class QJsonArray;

namespace net {
	
class LoginHandler;
struct ServerReply;

/**
 * The client for accessing the drawing server.
 */
class Client : public QObject {
	Q_OBJECT
public:
	explicit Client(QObject *parent=nullptr);

	/**
	 * @brief Connect to a remote server
	 * @param loginhandler the login handler to use
	 */
	void connectToServer(LoginHandler *loginhandler);

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

	/**
	 * @brief Get the number of bytes waiting to be sent
	 * @return upload queue length
	 */
	int uploadQueueBytes() const;

	//! Are we expecting more incoming data?
	bool isFullyCaughtUp() const { return m_catchupTo == 0; }

public slots:
	/**
	 * @brief Send one or more message to the server
	 *
	 * If the envelope starts with a Command type message, drawingCommandLocal
	 * will be emitted with a copy of the envelope before the message is sent.
	 */
	void sendEnvelope(const Envelope &envelope);

	/**
	 * @brief Send the reset image to the server
	 *
	 * The difference between this and sendEnvelope is that drawingCommandLocal
	 * will not be emitted, as the reset image was generate from content already
	 * on the canvas.
	 */
	void sendResetEnvelope(const net::Envelope &resetImage);

signals:
	void messageReceived(const Envelope &e);
	void drawingCommandLocal(const Envelope &e);
	void catchupProgress(int percentage);

	void needSnapshot();
	void sessionResetted();
	void sessionConfChange(const QJsonObject &config);

	void serverConnected(const QString &address, int port);
	void serverLoggedin(bool join);
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

private slots:
	void handleEnvelope(const Envelope &envelope);
	void handleConnect(const QUrl &url, uint8_t userid, bool join, bool auth, bool moderator, bool supportsAutoReset);
	void handleDisconnect(const QString &message, const QString &errorcode, bool localDisconnect);

private:
	void handleServerReply(const ServerReply &msg);
	void handleResetRequest(const ServerReply &msg);

	Server *m_server = nullptr;

	QUrl m_lastUrl;
	uint8_t m_myId = 1;
	bool m_moderator = false;
	bool m_isAuthenticated = false;
	bool m_supportsAutoReset = false;

	int m_catchupTo = 0;
	int m_caughtUp = 0;
	int m_catchupProgress = 0;
};

}

#endif
