/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "libserver/thinserverclient.h"
#include "libshared/net/messagequeue.h"
#include "libserver/thinsession.h"

namespace server {

ThinServerClient::ThinServerClient(QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: Client(socket, logger, parent)
	, m_historyPosition(-1)
{
	connect(messageQueue(), &protocol::MessageQueue::allSent,
		this, &ThinServerClient::sendNextHistoryBatch);
}

void ThinServerClient::sendNextHistoryBatch()
{
	// Only enqueue messages for uploading when upload queue is empty
	// and session is in a normal running state.
	// (We'll get another messagesAvailable signal when ready)
	if(session() == nullptr || messageQueue()->isUploading() || session()->state() != Session::State::Running)
		return;

	protocol::MessageList batch;
	int batchLast;
	std::tie(batch, batchLast) = session()->history()->getBatch(m_historyPosition);
	m_historyPosition = batchLast;
	messageQueue()->send(batch);

	static_cast<ThinSession*>(session())->cleanupHistoryCache();
}

}
