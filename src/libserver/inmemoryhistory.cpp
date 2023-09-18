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
{
}

std::tuple<net::MessageList, int> InMemoryHistory::getBatch(int after) const
{
	if(after >= lastIndex())
		return std::make_tuple(net::MessageList(), lastIndex());

	const int offset = qMax(0, after - firstIndex() + 1);
	Q_ASSERT(offset < m_history.size());

	return std::make_tuple(m_history.mid(offset), lastIndex());
}

void InMemoryHistory::historyAdd(const net::Message &msg)
{
	m_history.append(msg);
}

void InMemoryHistory::historyReset(const net::MessageList &newHistory)
{
	m_history = newHistory;
}

}
