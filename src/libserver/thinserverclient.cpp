// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/thinserverclient.h"
#include "libserver/thinsession.h"
#include "libshared/net/messagequeue.h"

namespace server {

ThinServerClient::ThinServerClient(
	QTcpSocket *socket, ServerLog *logger, QObject *parent)
	: Client(socket, logger, parent)
	, m_historyPosition(-1)
{
	connect(
		messageQueue(), &net::MessageQueue::allSent, this,
		&ThinServerClient::sendNextHistoryBatch);
}

ThinServerClient::~ThinServerClient()
{
	emit thinServerClientDestroyed(this);
}

void ThinServerClient::sendNextHistoryBatch()
{
	// Only enqueue messages for uploading when upload queue is empty
	// and session is in a normal running state.
	// (We'll get another messagesAvailable signal when ready)
	if(session() == nullptr || messageQueue()->isUploading() ||
	   session()->state() != Session::State::Running)
		return;

	net::MessageList batch;
	int batchLast;
	std::tie(batch, batchLast) =
		session()->history()->getBatch(m_historyPosition);
	m_historyPosition = batchLast;
	messageQueue()->sendMultiple(batch.size(), batch.constData());

	static_cast<ThinSession *>(session())->cleanupHistoryCache();
}

}
