// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/thinserverclient.h"
#include "libserver/thinsession.h"
#include "libshared/net/messagequeue.h"

namespace server {

ThinServerClient::ThinServerClient(
	QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: Client(socket, logger, false, parent)
	, m_historyPosition(-1LL)
{
	connectSendNextHistoryBatch();
}

#ifdef HAVE_WEBSOCKETS
ThinServerClient::ThinServerClient(
	QWebSocket *socket, const QHostAddress &ip, ServerLog *logger,
	QObject *parent)
	: Client(socket, ip, logger, false, parent)
	, m_historyPosition(-1LL)
{
	connectSendNextHistoryBatch();
}
#endif

ThinServerClient::~ThinServerClient()
{
	emit thinServerClientDestroyed(this);
}

void ThinServerClient::sendNextHistoryBatch()
{
	ThinSession *s = static_cast<ThinSession *>(session());
	net::MessageQueue *mq = messageQueue();
	// Only enqueue messages for uploading when upload queue is empty
	// and session is in a normal running state.
	// (We'll get another messagesAvailable signal when ready)
	if(s && !mq->isUploading() && s->state() == Session::State::Running) {
		// There may be a streamed reset pending, waiting for clients to catch
		// up far enough. If the streamed reset is applies, it will change the
		// history position of all clients, so don't touch it before this point!
		s->resolvePendingStreamedReset();

		net::MessageList batch;
		long long batchLast;
		std::tie(batch, batchLast) = s->history()->getBatch(m_historyPosition);
		m_historyPosition = batchLast;
		mq->sendMultiple(batch.size(), batch.constData());

		s->cleanupHistoryCache();
	}
}

void ThinServerClient::connectSendNextHistoryBatch()
{
	connect(
		messageQueue(), &net::MessageQueue::allSent, this,
		&ThinServerClient::sendNextHistoryBatch);
}

}
