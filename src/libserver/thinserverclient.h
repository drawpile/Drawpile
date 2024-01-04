// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef THINSERVERCLIENT_H
#define THINSERVERCLIENT_H
#include "libserver/client.h"
#include "libserver/serverconfig.h"

class QHostAddress;

namespace server {

class ThinServerClient final : public Client {
	Q_OBJECT
public:
	ThinServerClient(
		QTcpSocket *tcpSocket, ServerLog *logger, QObject *parent = nullptr);
#ifdef HAVE_WEBSOCKETS
	ThinServerClient(
		QWebSocket *webSocket, const QHostAddress &ip, ServerLog *logger,
		QObject *parent = nullptr);
#endif
	~ThinServerClient() override;

	/**
	 * @brief Get this client's position in the session history
	 * The returned index in the index of the last history message that
	 * is (or was) in the client's upload queue.
	 */
	int historyPosition() const { return m_historyPosition; }

	void setHistoryPosition(int pos) { m_historyPosition = pos; }

signals:
	void thinServerClientDestroyed(ThinServerClient *thisClient);

public slots:
	void sendNextHistoryBatch();

private:
	void connectSendNextHistoryBatch();

	int m_historyPosition;
};

}

#endif // THINSERVERCLIENT_H
