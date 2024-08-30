// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/reset_stream.h>
}
#include "libserver/client.h"
#include "libserver/sessionhistory.h"
#include "libshared/net/servercmd.h"

namespace server {

SessionHistory::SessionHistory(const QString &id, QObject *parent)
	: QObject(parent)
	, m_id(id)
	, m_startTime(QDateTime::currentDateTimeUtc())
	, m_sizeInBytes(0)
	, m_sizeLimit(0)
	, m_autoResetBaseSize(0)
	, m_firstIndex(0)
	, m_lastIndex(-1)
{
}

bool SessionHistory::hasSpaceFor(size_t bytes, size_t extra) const
{
	return m_sizeLimit <= 0 || m_sizeInBytes + bytes <= m_sizeLimit + extra;
}

bool SessionHistory::addBan(
	const QString &username, const QHostAddress &ip, const QString &extAuthId,
	const QString &sid, const QString &bannedBy, const Client *client)
{
	int id;
	if(client) {
		const SessionBanner banner = {
			client->username(),
			client->authId(),
			client->peerAddress(),
			client->sid(),
		};
		id = m_banlist.addBan(
			username, ip, extAuthId, sid, bannedBy, 0, &banner);
	} else {
		id = m_banlist.addBan(username, ip, extAuthId, sid, bannedBy);
	}

	if(id > 0) {
		historyAddBan(id, username, ip, extAuthId, sid, bannedBy);
		return true;
	}
	return false;
}

bool SessionHistory::importBans(
	const QJsonObject &data, int &outTotal, int &outImported,
	const Client *client)
{
	outTotal = 0;
	outImported = 0;
	return SessionBanList::importBans(data, [&](const SessionBan &b) {
		++outTotal;
		if(addBan(b.username, b.ip, b.authId, b.sid, b.bannedBy, client)) {
			++outImported;
		}
	});
}

QString SessionHistory::removeBan(int id)
{
	QString unbanned = m_banlist.removeBan(id);
	if(!unbanned.isEmpty())
		historyRemoveBan(id);
	return unbanned;
}

void SessionHistory::joinUser(uint8_t id, const QString &name)
{
	idQueue().setIdForName(id, name);
}

void SessionHistory::historyLoaded(size_t size, int messageCount)
{
	Q_ASSERT(m_lastIndex == -1);
	m_sizeInBytes = size;
	m_lastIndex = messageCount - 1;
	m_autoResetBaseSize = size;
}

bool SessionHistory::addMessage(const net::Message &msg)
{
	size_t bytes = msg.length();
	if(hasRegularSpaceFor(bytes)) {
		addMessageInternal(msg, bytes);
		emit newMessagesAvailable();
		return true;
	} else {
		return false;
	}
}

bool SessionHistory::addEmergencyMessage(const net::Message &msg)
{
	uint bytes = uint(msg.length());
	if(hasEmergencySpaceFor(bytes)) {
		addMessageInternal(msg, bytes);
		emit newMessagesAvailable();
		return true;
	} else {
		return false;
	}
}

void SessionHistory::addMessageInternal(const net::Message &msg, size_t bytes)
{
	m_sizeInBytes += bytes;
	++m_lastIndex;
	historyAdd(msg);
}

bool SessionHistory::reset(const net::MessageList &newHistory)
{
	size_t newSize = 0;
	for(const net::Message &msg : newHistory) {
		newSize += msg.length();
	}
	if(m_sizeLimit > 0 && newSize > m_sizeLimit) {
		return false;
	}

	abortStreamedReset();
	m_sizeInBytes = newSize;
	m_firstIndex = m_lastIndex + 1LL;
	m_lastIndex += newHistory.size();
	resetAutoResetThresholdBase();
	historyReset(newHistory);
	emit newMessagesAvailable();
	return true;
}

StreamResetStartResult SessionHistory::startStreamedReset(
	uint8_t ctxId, const QString &correlator,
	const net::MessageList &serverSideStateMessages)
{
	if(m_resetStreamState != ResetStreamState::None) {
		return StreamResetStartResult::AlreadyActive;
	}

	net::Message softResetMsg = net::makeSoftResetMessage(0);
	net::Message resetStartMsg =
		net::ServerReply::makeStreamedResetStart(ctxId, correlator);
	size_t softResetBytes = softResetMsg.length();
	size_t resetStartBytes = resetStartMsg.length();
	if(!hasRegularSpaceFor(softResetBytes + resetStartBytes)) {
		return StreamResetStartResult::OutOfSpace;
	}

	addMessageInternal(softResetMsg, softResetBytes);
	addMessageInternal(resetStartMsg, resetStartBytes);

	StreamResetStartResult result = openResetStream(serverSideStateMessages);
	if(result == StreamResetStartResult::Ok) {
		m_resetStreamState = ResetStreamState::Streaming;
		m_resetStreamCtxId = ctxId;
		m_resetStreamSize = 0;
		m_resetStreamStartIndex = m_lastIndex + 1LL;
		m_resetStreamMessageCount = 0;
	}

	emit newMessagesAvailable();
	return result;
}

bool SessionHistory::receiveResetStreamMessageCallback(
	void *user, DP_Message *msg)
{
	return static_cast<SessionHistory *>(user)->receiveResetStreamMessage(
		net::Message::noinc(msg));
}

bool SessionHistory::receiveResetStreamMessage(const net::Message &msg)
{
	if(msg.isControl() || (msg.isServerMeta() && msg.type() != DP_MSG_CHAT)) {
		m_resetStreamAddError = StreamResetAddResult::DisallowedType;
		return false;
	}

	size_t newSize = m_resetStreamSize + msg.length();
	if(m_sizeLimit > 0 && newSize > m_sizeLimit) {
		m_resetStreamAddError = StreamResetAddResult::OutOfSpace;
		return false;
	}
	m_resetStreamSize = newSize;

	StreamResetAddResult result = addResetStreamMessage(msg);
	if(result == StreamResetAddResult::Ok) {
		++m_resetStreamMessageCount;
		return true;
	} else {
		m_resetStreamAddError = result;
		return false;
	}
}

StreamResetAddResult
SessionHistory::addStreamResetMessage(uint8_t ctxId, const net::Message &msg)
{
	if(m_resetStreamState != ResetStreamState::Streaming) {
		return StreamResetAddResult::NotActive;
	}

	if(m_resetStreamCtxId != ctxId) {
		return StreamResetAddResult::InvalidUser;
	}

	if(msg.type() != DP_MSG_RESET_STREAM) {
		return StreamResetAddResult::BadType;
	}

	size_t size;
	const unsigned char *data =
		DP_msg_reset_stream_data(msg.toResetStream(), &size);
	if(size != 0) {
		if(!m_resetStreamConsumer) {
			m_resetStreamConsumer = DP_reset_stream_consumer_new(
				&receiveResetStreamMessageCallback, this, false);
			if(!m_resetStreamConsumer) {
				abortActiveStreamedReset();
				return StreamResetAddResult::ConsumerError;
			}
		}

		m_resetStreamAddError = StreamResetAddResult::ConsumerError;
		if(!DP_reset_stream_consumer_push(m_resetStreamConsumer, data, size)) {
			Q_ASSERT(m_resetStreamAddError != StreamResetAddResult::Ok);
			return m_resetStreamAddError;
		}
	}
	return StreamResetAddResult::Ok;
}

StreamResetAbortResult SessionHistory::abortStreamedReset(int ctxId)
{
	if(m_resetStreamState == ResetStreamState::Streaming) {
		if(ctxId < 0 || ctxId == m_resetStreamCtxId) {
			abortActiveStreamedReset();
			return StreamResetAbortResult::Ok;
		} else {
			return StreamResetAbortResult::InvalidUser;
		}
	} else {
		return StreamResetAbortResult::NotActive;
	}
}

StreamResetPrepareResult
SessionHistory::prepareStreamedReset(uint8_t ctxId, int expectedMessageCount)
{
	if(m_resetStreamState != ResetStreamState::Streaming) {
		return StreamResetPrepareResult::NotActive;
	}

	if(m_resetStreamCtxId != ctxId) {
		return StreamResetPrepareResult::InvalidUser;
	}

	m_resetStreamAddError = StreamResetAddResult::ConsumerError;
	bool freeOk = DP_reset_stream_consumer_free_finish(m_resetStreamConsumer);
	m_resetStreamConsumer = nullptr;
	if(!freeOk) {
		switch(m_resetStreamAddError) {
		case StreamResetAddResult::OutOfSpace:
			return StreamResetPrepareResult::OutOfSpace;
		default:
			return StreamResetPrepareResult::ConsumerError;
		}
	}

	if(m_resetStreamMessageCount != expectedMessageCount ||
	   expectedMessageCount == 0) {
		abortActiveStreamedReset();
		return StreamResetPrepareResult::InvalidMessageCount;
	}

	switch(addResetStreamMessage(net::ServerReply::makeCaughtUp(0))) {
	case StreamResetAddResult::Ok:
		break;
	case StreamResetAddResult::OutOfSpace:
		return StreamResetPrepareResult::OutOfSpace;
	default:
		return StreamResetPrepareResult::ConsumerError;
	}

	StreamResetPrepareResult result = prepareResetStream();
	if(result == StreamResetPrepareResult::Ok) {
		m_resetStreamState = ResetStreamState::Prepared;
	} else {
		m_resetStreamState = ResetStreamState::None;
	}

	m_resetStreamCtxId = 0;
	return result;
}

bool SessionHistory::resolveStreamedReset(
	long long &outOffset, QString &outError)
{
	if(m_resetStreamState != ResetStreamState::Prepared) {
		outError = QStringLiteral("reset stream is not prepared");
		return false;
	}

	long long newFirstIndex = m_lastIndex + 1LL;
	long long messageCount;
	size_t sizeInBytes;
	bool ok =
		resolveResetStream(newFirstIndex, messageCount, sizeInBytes, outError);
	m_resetStreamState = ResetStreamState::None;
	m_resetStreamCtxId = 0;
	if(!ok) {
		return false;
	}

	m_sizeInBytes = sizeInBytes;
	m_firstIndex = newFirstIndex;
	m_lastIndex += messageCount;
	m_autoResetBaseSize = m_resetStreamSize;
	outOffset = messageCount;
	return true;
}

void SessionHistory::abortActiveStreamedReset()
{
	discardResetStream();
	m_resetStreamState = ResetStreamState::None;
	m_resetStreamCtxId = 0;
	DP_reset_stream_consumer_free_discard(m_resetStreamConsumer);
	m_resetStreamConsumer = nullptr;
}

size_t SessionHistory::effectiveAutoResetThreshold() const
{
	size_t t = autoResetThreshold();
	// Zero means autoreset is not enabled
	if(t > 0) {
		t += m_autoResetBaseSize;
		if(m_sizeLimit > 0) {
			t = qMin(t, size_t(m_sizeLimit * 0.9));
		}
	}
	return t;
}

void SessionHistory::resetAutoResetThresholdBase()
{
	m_autoResetBaseSize = m_sizeInBytes;
}

void SessionHistory::setAuthenticatedOperator(const QString &authId, bool op)
{
	if(op) {
		Q_ASSERT(!authId.isEmpty());
		m_authOps.insert(authId);
	} else {
		m_authOps.remove(authId);
	}
}

void SessionHistory::setAuthenticatedTrust(const QString &authId, bool trusted)
{
	if(trusted) {
		Q_ASSERT(!authId.isEmpty());
		m_authTrusted.insert(authId);
	} else {
		m_authTrusted.remove(authId);
	}
}

void SessionHistory::setAuthenticatedUsername(
	const QString &authId, const QString &username)
{
	Q_ASSERT(!authId.isEmpty());
	Q_ASSERT(!username.isEmpty());
	m_authUsernames.insert(authId, username);
}

const QString *SessionHistory::authenticatedUsernameFor(const QString &authId)
{
	QHash<QString, QString>::const_iterator it = m_authUsernames.find(authId);
	return it == m_authUsernames.constEnd() ? nullptr : &it.value();
}

int SessionHistory::incrementNextCatchupKey(int &nextCatchupKey)
{
	int result = nextCatchupKey;
	// Wrap around the catchup key at an arbitrary, but plenty large value.
	nextCatchupKey = result < MAX_CATCHUP_KEY ? result + 1 : MIN_CATCHUP_KEY;
	return result;
}

}
