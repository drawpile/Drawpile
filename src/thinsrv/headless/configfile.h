/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
	bool isAddressBanned(const QHostAddress &addr) const override;
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
	mutable QList<QPair<QHostAddress, int>> m_banlist;
	mutable QList<QUrl> m_announcewhitelist;
	mutable QDateTime m_lastmod;
};

}

#endif
