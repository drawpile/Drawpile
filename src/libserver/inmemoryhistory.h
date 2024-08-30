// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_SERVER_SESSION_INMEMHISTORY_H
#define DP_SERVER_SESSION_INMEMHISTORY_H
#include "libserver/sessionhistory.h"
#include "libshared/net/protover.h"
#include <QSet>

namespace server {

/**
 * @brief A session history backend that stores the session in memory
 */
class InMemoryHistory final : public SessionHistory {
	Q_OBJECT
public:
	InMemoryHistory(
		const QString &id, const QString &alias,
		const protocol::ProtocolVersion &version, const QString &founder,
		QObject *parent = nullptr);

	std::tuple<net::MessageList, long long>
	getBatch(long long after) const override;

	void terminate() override
	{
		// nothing to do
	}

	void cleanupBatches(long long) override
	{
		// no caching, nothing to do
	}

	QString idAlias() const override { return m_alias; }
	QString founderName() const override { return m_founder; }
	void setFounderName(const QString &founder) override
	{
		m_founder = founder;
	}
	protocol::ProtocolVersion protocolVersion() const override
	{
		return m_version;
	}
	QByteArray passwordHash() const override { return m_password; }
	void setPasswordHash(const QByteArray &password) override
	{
		m_password = password;
	}
	QByteArray opwordHash() const override { return m_opword; }
	void setOpwordHash(const QByteArray &opword) override { m_opword = opword; }
	int maxUsers() const override { return m_maxUsers; }
	void setMaxUsers(int max) override { m_maxUsers = qBound(1, max, 254); }
	QString title() const override { return m_title; }
	void setTitle(const QString &title) override { m_title = title; }
	Flags flags() const override { return m_flags; }
	void setFlags(Flags f) override { m_flags = f; }
	void setAutoResetThreshold(size_t limit) override
	{
		if(sizeLimit() == 0)
			m_autoReset = limit;
		else
			m_autoReset = qMin(size_t(sizeLimit() * 0.9), limit);
	}
	size_t autoResetThreshold() const override { return m_autoReset; }
	int nextCatchupKey() override;

	void addAnnouncement(const QString &url) override
	{
		m_announcements.insert(url);
	}
	void removeAnnouncement(const QString &url) override
	{
		m_announcements.remove(url);
	}
	QStringList announcements() const override
	{
		return m_announcements.values();
	}

protected:
	void historyAdd(const net::Message &msg) override;
	void historyReset(const net::MessageList &newHistory) override;
	void historyAddBan(
		int, const QString &, const QHostAddress &, const QString &,
		const QString &, const QString &) override
	{ /* not persistent */
	}
	void historyRemoveBan(int) override { /* not persistent */ }

	StreamResetStartResult
	openResetStream(const net::MessageList &serverSideStateMessages) override;
	StreamResetAddResult
	addResetStreamMessage(const net::Message &msg) override;
	StreamResetPrepareResult prepareResetStream() override;
	bool resolveResetStream(
		long long newFirstIndex, long long &outMessageCount,
		size_t &outSizeInBytes, QString &outError) override;
	void discardResetStream() override;

private:
	net::MessageList m_history;
	QSet<QString> m_announcements;
	QString m_alias;
	QString m_founder;
	QString m_title;
	protocol::ProtocolVersion m_version;
	QByteArray m_password;
	QByteArray m_opword;
	int m_maxUsers;
	size_t m_autoReset;
	Flags m_flags;
	int m_nextCatchupKey;
	int m_resetStreamIndex = -1;
	net::MessageList m_resetStream;
};

}

#endif
