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

	std::tuple<net::MessageList, int> getBatch(int after) const override;

	void terminate() override
	{
		// nothing to do
	}

	void cleanupBatches(int) override
	{
		// no caching, nothing to do
	}

	QString idAlias() const override { return m_alias; }
	QString founderName() const override { return m_founder; }
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
	void setAutoResetThreshold(uint limit) override
	{
		if(sizeLimit() == 0)
			m_autoReset = limit;
		else
			m_autoReset = qMin(uint(sizeLimit() * 0.9), limit);
	}
	uint autoResetThreshold() const override { return m_autoReset; }

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

	void setAuthenticatedOperator(const QString &authId, bool op) override
	{
		if(authId.isEmpty())
			return;
		if(op)
			m_ops.insert(authId);
		else
			m_ops.remove(authId);
	}
	void setAuthenticatedTrust(const QString &authId, bool trusted) override
	{
		if(authId.isEmpty())
			return;
		if(trusted)
			m_trusted.insert(authId);
		else
			m_trusted.remove(authId);
	}
	bool isOperator(const QString &authId) const override
	{
		return m_ops.contains(authId);
	}
	bool isTrusted(const QString &authId) const override
	{
		return m_trusted.contains(authId);
	}
	bool isAuthenticatedOperators() const override { return !m_ops.isEmpty(); }

protected:
	void historyAdd(const net::Message &msg) override;
	void historyReset(const net::MessageList &newHistory) override;
	void historyAddBan(
		int, const QString &, const QHostAddress &, const QString &,
		const QString &, const QString &) override
	{ /* not persistent */
	}
	void historyRemoveBan(int) override
	{ /* not persistent */
	}

private:
	net::MessageList m_history;
	QSet<QString> m_ops;
	QSet<QString> m_trusted;
	QSet<QString> m_announcements;
	QString m_alias;
	QString m_founder;
	QString m_title;
	protocol::ProtocolVersion m_version;
	QByteArray m_password;
	QByteArray m_opword;
	int m_maxUsers;
	uint m_autoReset;
	Flags m_flags;
};

}

#endif
