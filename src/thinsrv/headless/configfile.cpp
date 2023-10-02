// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/headless/configfile.h"
#include "libserver/serverlog.h"
#include "libshared/util/passwordhash.h"

#include <QFileInfo>

namespace server {

ConfigFile::ConfigFile(const QString &path, QObject *parent)
	: ServerConfig(parent),
	  m_path(path),
	  m_logger(new InMemoryLog)
{
	// When the configuration file is compiled in as a qresource,
	// lastModified() always returns a null datetime. We still
	// want to read the configuration file once, though.
	// That can be accomplished by setting the initial lastmod to some
	// unlikely non-null datetime.
	m_lastmod = QDateTime::fromMSecsSinceEpoch(1);
}

ConfigFile::~ConfigFile()
{
	delete m_logger;
}

bool ConfigFile::isModified() const
{
	QFileInfo f(m_path);
	if(!f.exists())
		return false;

	return f.lastModified() != m_lastmod;
}

void ConfigFile::reloadFile() const
{
	QFile f(m_path);
	if(!f.open(QFile::ReadOnly | QFile::Text)) {
		qCritical("%s: Error %s", qPrintable(m_path), qPrintable(f.errorString()));
		return;
	}

	m_lastmod = QFileInfo(f).lastModified();

	QTextStream in(&f);

	m_config.clear();
	m_ipbans.clear();
	m_systembans.clear();
	m_userbans.clear();
	m_announcewhitelist.clear();
	m_users.clear();

	enum { CONFIG, IPBANS, SYSTEMBANS, USERBANS, AWL, USERS } section = CONFIG;

	while(!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		if(line.at(0) == '[') {
			if(line.compare("[config]", Qt::CaseInsensitive)==0)
				section = CONFIG;
			else if(line.compare("[ipbans]", Qt::CaseInsensitive)==0)
				section = IPBANS;
			else if(line.compare("[systembans]", Qt::CaseInsensitive)==0)
				section = SYSTEMBANS;
			else if(line.compare("[userbans]", Qt::CaseInsensitive)==0)
				section = USERBANS;
			else if(line.compare("[announcewhitelist]", Qt::CaseInsensitive)==0)
				section = AWL;
			else if(line.compare("[users]", Qt::CaseInsensitive)==0)
				section = USERS;
			else
				qWarning("Unknown configuration file section: %s", qPrintable(line));
			continue;
		}

		if(section == CONFIG) {
			int sep = line.indexOf('=');
			if(sep<1) {
				qWarning("Invalid setting line: %s", qPrintable(line));
				continue;
			}

			QString key = line.left(sep).trimmed();
			QString value = line.mid(sep+1).trimmed();

			m_config[key] = value;

		} else if(section == IPBANS) {
			int sep = line.indexOf('/');
			QString ip, subnet;
			if(sep<0) {
				ip = line;
				subnet = QString();
			} else {
				ip = line.left(sep);
				subnet = line.mid(sep+1);
			}

			QHostAddress ipaddr(ip);
			if(ipaddr.isNull()) {
				qWarning("Invalid IP address: %s", qPrintable(ip));
				continue;
			}

			m_ipbans << QPair<QHostAddress,int> { ipaddr, subnet.toInt() };

		} else if(section == SYSTEMBANS) {
			int sep = line.indexOf(':');
			QString sid;
			BanReaction reaction;
			if(sep < 0) {
				sid = line;
				reaction = BanReaction::NormalBan;
			} else {
				sid = line.left(sep);
				reaction = parseReaction(line.mid(sep + 1));
			}

			if(m_systembans.contains(sid)) {
				qWarning(
					"Duplicate system ban with sid '%s'", qUtf8Printable(sid));
			}
			int sourceId = m_systembans.size() + 1;
			m_systembans[sid] = {
				reaction,
				QString(),
				QDateTime(),
				sid,
				QStringLiteral("configfile"),
				QStringLiteral("SID"),
				sourceId,
				false};

		} else if(section == USERBANS) {
			int sep = line.indexOf(':');
			QString userIdString;
			BanReaction reaction;
			if(sep < 0) {
				userIdString = line;
				reaction = BanReaction::NormalBan;
			} else {
				userIdString = line.left(sep);
				reaction = parseReaction(line.mid(sep + 1));
			}

			bool ok;
			long long userId = userIdString.toLongLong(&ok);
			if(ok) {
				if(m_userbans.contains(userId)) {
					qWarning("Duplicate user ban with user id '%lld'", userId);
				}
				int sourceId = m_userbans.size() + 1;
				m_userbans[userId] = {
					reaction,
					QString(),
					QDateTime(),
					QString::number(userId),
					QStringLiteral("configfile"),
					QStringLiteral("User"),
					sourceId,
					false};
			} else {
				qWarning("Invalid user id '%s'", qUtf8Printable(userIdString));
			}

		} else if(section == AWL) {
			QUrl url(line);
			if(!url.isValid()) {
				qWarning("Invalid URL: %s", qPrintable(line));
				continue;
			}

			m_announcewhitelist << url;

		} else if(section == USERS) {
			QStringList userline = line.split(':');
			if(userline.length() != 3) {
				qWarning("%s: line should have three parts: username:password:flags", qPrintable(line));
				continue;
			}

			QByteArray hash = userline.at(1).toUtf8();
			if(!passwordhash::isValidHash(hash)) {
				qWarning("%s: invalid password hash", qPrintable(userline.at(1)));
				continue;
			}

			m_users[userline.at(0)] = User { hash, userline.at(2).split(',') };
		}
	}
}

QString ConfigFile::getConfigValue(const ConfigKey key, bool &found) const
{
	if(isModified())
		reloadFile();

	if(m_config.count(key.name)==0) {
		found = false;
		return QString();
	} else {
		found = true;
		return m_config[key.name];
	}
}

BanResult ConfigFile::isAddressBanned(const QHostAddress &addr) const
{
	if(isModified()) {
		reloadFile();
	}

	int banCount = m_ipbans.size();
	for(int i = 0; i < banCount; ++i) {
		const QPair<QHostAddress, int> &ipban = m_ipbans[i];
		if(matchesBannedAddress(addr, ipban.first, ipban.second)) {
			return {
				BanReaction::NormalBan, QString(), QDateTime(),
				addr.toString(), QStringLiteral("configfile"),
				QStringLiteral("IP"), i + 1, true};
		}
	}

	return ServerConfig::isAddressBanned(addr);
}

BanResult ConfigFile::isSystemBanned(const QString &sid) const
{
	if(isModified()) {
		reloadFile();
	}

	if(m_systembans.contains(sid)) {
		return m_systembans[sid];
	} else {
		return ServerConfig::isSystemBanned(sid);
	}
}

BanResult ConfigFile::isUserBanned(long long userId) const
{
	if(isModified()) {
		reloadFile();
	}

	if(m_userbans.contains(userId)) {
		return m_userbans[userId];
	} else {
		return ServerConfig::isUserBanned(userId);
	}
}

bool ConfigFile::isAllowedAnnouncementUrl(const QUrl &url) const
{
	if(!getConfigBool(config::AnnounceWhiteList))
		return true;

	return m_announcewhitelist.contains(url);
}

RegisteredUser ConfigFile::getUserAccount(const QString &username, const QString &password) const
{
	if(m_users.contains(username)) {
		const User &u = m_users[username];
		if(u.password.startsWith("*")) {
			return RegisteredUser {
				RegisteredUser::Banned,
				username,
				QStringList(),
				username
			};

		} else if(!passwordhash::check(password, u.password)) {
			return RegisteredUser {
				RegisteredUser::BadPass,
				username,
				QStringList(),
				username
			};
		} else {
			return RegisteredUser {
				RegisteredUser::Ok,
				username,
				u.flags,
				username
			};
		}

	} else {
		return RegisteredUser {
			RegisteredUser::NotFound,
			username,
			QStringList(),
			QString()
		};
	}
}

void ConfigFile::setConfigValue(const ConfigKey key, const QString &value)
{
	Q_UNUSED(value);
	qDebug("not setting value for key %s", qPrintable(key.name));
}

}
