// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef THINSERVERCLIENT_H
#define THINSERVERCLIENT_H

#include "libserver/client.h"

namespace server {

class ThinServerClient final : public Client
{
public:
	ThinServerClient(QTcpSocket *socket, ServerLog *logger, QObject *parent=nullptr);

	/**
	 * @brief Get this client's position in the session history
	 * The returned index in the index of the last history message that
	 * is (or was) in the client's upload queue.
	 */
	int historyPosition() const { return m_historyPosition; }

	void setHistoryPosition(int pos) { m_historyPosition = pos; }

public slots:
	void sendNextHistoryBatch();

private:
	int m_historyPosition;
};

}

#endif // THINSERVERCLIENT_H
