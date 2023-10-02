// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include "libserver/serverconfig.h"

#include <QDateTime>
#include <QHostAddress>
#include <QUrl>

namespace server {

/**
 * @brief Server configuration read from a text file.
 *
 * The file is automatically reloaded if it has changed.
 *
 * Format is simple:
 *
 *     [config]
 *     key = value
 *
 *     [ipbans]
 *     192.168.0.0/32
 *
 *     [announceWhiteList]
 *     https://drawpile.net/api/listing/
 *
 *     [users]
 *     username:plain;password:MOD
 *
 * The default section is [config], so the header can be omitted.
 */
class ConfigFile final : public ServerConfig
{
	Q_OBJECT
public:
	explicit ConfigFile(const QString &path, QObject *parent=nullptr);
	~ConfigFile() override;

	bool isModified() const;

	bool isAllowedAnnouncementUrl(const QUrl &url) const override;
	BanResult isAddressBanned(const QHostAddress &addr) const override;
	BanResult isSystemBanned(const QString &sid) const override;
	BanResult isUserBanned(long long userId) const override;
	RegisteredUser getUserAccount(const QString &username, const QString &password) const override;

	ServerLog *logger() const override { return m_logger; }

protected:
	QString getConfigValue(const ConfigKey key, bool &found) const override;
	void setConfigValue(const ConfigKey key, const QString &value) override;

private:
	void reloadFile() const;

	QString m_path;
	ServerLog *m_logger;

	struct User {
		QByteArray password;
		QStringList flags;
	};

	// Cached settings:
	mutable QHash<QString, QString> m_config;
	mutable QHash<QString, User> m_users;
	mutable QList<QPair<QHostAddress, int>> m_ipbans;
	mutable QHash<QString, BanResult> m_systembans;
	mutable QHash<long long, BanResult> m_userbans;
	mutable QList<QUrl> m_announcewhitelist;
	mutable QDateTime m_lastmod;
};

}

#endif
