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

#include "configfile.h"
#include "../shared/util/logger.h"

#include <QFileInfo>

namespace server {

ConfigFile::ConfigFile(const QString &path, QObject *parent)
	: ServerConfig(parent),
	  m_path(path)
{
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
		logger::error() << m_path << "Error:" << f.errorString();
		return;
	}

	m_lastmod = QFileInfo(f).lastModified();

	QTextStream in(&f);

	m_config.clear();
	m_banlist.clear();
	m_announcewhitelist.clear();

	enum { CONFIG, BANLIST, AWL } section = CONFIG;

	while(!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if(line.isEmpty() || line.at(0) == '#')
			continue;

		if(line.at(0) == '[') {
			if(line.compare("[config]", Qt::CaseInsensitive)==0)
				section = CONFIG;
			else if(line.compare("[ipbans]", Qt::CaseInsensitive)==0)
				section = BANLIST;
			else if(line.compare("[announcewhitelist]", Qt::CaseInsensitive)==0)
				section = AWL;
			else
				logger::warning() << "Unknown configuration file section:" << line;
			continue;
		}

		if(section == CONFIG) {
			int sep = line.indexOf('=');
			if(sep<1) {
				logger::warning() << "Invalid setting line:" << line;
				continue;
			}

			QString key = line.left(sep).trimmed();
			QString value = line.mid(sep+1).trimmed();

			m_config[key] = value;

		} else if(section == BANLIST) {
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
				logger::warning() << "Invalid IP address:" << ip;
				continue;
			}

			m_banlist << QPair<QHostAddress,int> { ipaddr, subnet.toInt() };

		} else if(section == AWL) {
			QUrl url(line);
			if(!url.isValid()) {
				logger::warning() << "Invalid URL:" << line;
				continue;
			}

			m_announcewhitelist << url;
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

bool ConfigFile::isAddressBanned(const QHostAddress &addr) const
{
	if(isModified())
		reloadFile();

	for(const QPair<QHostAddress,int> ipban : m_banlist) {
		QHostAddress subnet = ipban.first;
		int mask = ipban.second;

		if(mask==0) {
			switch(subnet.protocol()) {
			case QAbstractSocket::IPv4Protocol: mask=32; break;
			case QAbstractSocket::IPv6Protocol: mask=128; break;
			default: break;
			}
		}

		if(addr.isInSubnet(subnet, mask))
			return true;
	}

	return false;
}

bool ConfigFile::isAllowedAnnouncementUrl(const QUrl &url) const
{
	if(!getConfigBool(config::AnnounceWhiteList))
		return true;

	return m_announcewhitelist.contains(url);
}

void ConfigFile::setConfigValue(const ConfigKey key, const QString &value)
{
	Q_UNUSED(value);
	logger::debug() << "configFile: not setting value for key" << key.name;
}

}
