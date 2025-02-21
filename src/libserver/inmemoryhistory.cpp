// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/inmemoryhistory.h"
#include "libshared/util/passwordhash.h"

namespace server {

InMemoryHistory::InMemoryHistory(
	const QString &id, const QString &alias,
	const protocol::ProtocolVersion &version, const QString &founder,
	QObject *parent)
	: SessionHistory(id, parent)
	, m_alias(alias)
	, m_founder(founder)
	, m_version(version)
	, m_maxUsers(254)
	, m_autoReset(0)
	, m_flags()
	, m_nextCatchupKey(INITIAL_CATCHUP_KEY)
{
}

int InMemoryHistory::nextCatchupKey()
{
	return incrementNextCatchupKey(m_nextCatchupKey);
}

bool InMemoryHistory::isStreamResetIoAvailable() const
{
	return true;
}

qint64 InMemoryHistory::resetStreamForkPos() const
{
	return m_resetStreamIndex;
}

qint64 InMemoryHistory::resetStreamHeaderPos() const
{
	return 0;
}

std::tuple<net::MessageList, long long>
InMemoryHistory::getBatch(long long after) const
{
	if(after >= lastIndex())
		return std::make_tuple(net::MessageList(), lastIndex());

	const long long offset = qMax(0LL, after - firstIndex() + 1LL);
	Q_ASSERT(offset < m_history.size());

	return std::make_tuple(m_history.mid(offset), lastIndex());
}

void InMemoryHistory::historyAdd(const net::Message &msg)
{
	m_history.append(msg);
}

void InMemoryHistory::historyReset(const net::MessageList &newHistory)
{
	Q_ASSERT(m_resetStream.isEmpty());
	m_history = newHistory;
}

StreamResetStartResult InMemoryHistory::openResetStream(
	const net::MessageList &serverSideStateMessages)
{
	m_resetStream = serverSideStateMessages;
	m_resetStreamIndex = m_history.size();
	return StreamResetStartResult::Ok;
}

StreamResetAddResult
InMemoryHistory::addResetStreamMessage(const net::Message &msg)
{
	m_resetStream.append(msg);
	return StreamResetAddResult::Ok;
}

StreamResetPrepareResult InMemoryHistory::prepareResetStream()
{
	// Nothing to do here.
	return StreamResetPrepareResult::Ok;
}

bool InMemoryHistory::resolveResetStream(
	long long newFirstIndex, long long &outMessageCount, size_t &outSizeInBytes,
	QString &outError)
{
	Q_UNUSED(newFirstIndex);

	int end = m_history.size();
	Q_ASSERT(m_resetStreamIndex <= end);

	size_t sizeInBytes = 0;
	for(const net::Message &msg : m_resetStream) {
		sizeInBytes += msg.length();
	}
	for(int i = m_resetStreamIndex; i < end; ++i) {
		sizeInBytes += m_history[i].length();
	}
	outSizeInBytes = sizeInBytes;

	size_t sizeLimitInBytes = sizeLimit();
	if(sizeLimitInBytes == 0 || sizeInBytes <= sizeLimitInBytes) {
		for(int i = m_resetStreamIndex; i < end; ++i) {
			m_resetStream.append(m_history[i]);
		}
		m_history.clear();
		m_history.swap(m_resetStream);
		outMessageCount = m_history.size();
		return true;
	} else {
		outError = QStringLiteral("total size %1 exceeds limit %2")
					   .arg(sizeInBytes)
					   .arg(sizeLimitInBytes);
		return false;
	}
}

void InMemoryHistory::discardResetStream()
{
	m_resetStream.clear();
	m_resetStreamIndex = -1;
}

}
