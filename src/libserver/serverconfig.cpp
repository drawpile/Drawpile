/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2017 Calle Laakkonen

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

#include "libserver/serverconfig.h"

#include <QRegularExpression>

namespace server {

QString ServerConfig::getConfigString(ConfigKey key) const
{
	bool found;
	const QString val = getConfigValue(key, found);
	if(!found) {
		return key.defaultValue;
	}
	return val;
}

int ServerConfig::getConfigTime(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::TIME);
	const QString val = getConfigString(key);

	const int t = parseTimeString(val);
	Q_ASSERT(t>=0);
	return t;
}

int ServerConfig::getConfigSize(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::SIZE);
	const QString val = getConfigString(key);

	const int s = parseSizeString(val);
	Q_ASSERT(s>=0);
	return s;
}

int ServerConfig::getConfigInt(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::INT);
	const QString val = getConfigString(key);
	bool ok;
	int i = val.toInt(&ok);
	Q_ASSERT(ok);
	return i;
}

bool ServerConfig::getConfigBool(ConfigKey key) const
{
	Q_ASSERT(key.type == ConfigKey::BOOL);
	const QString val = getConfigString(key).toLower();
	return val == "1" || val == "true";
}

QVariant ServerConfig::getConfigVariant(ConfigKey key) const
{
	switch(key.type) {
	case ConfigKey::STRING: return getConfigString(key);
	case ConfigKey::TIME: return getConfigTime(key);
	case ConfigKey::SIZE: return getConfigSize(key);
	case ConfigKey::INT: return getConfigInt(key);
	case ConfigKey::BOOL: return getConfigBool(key);
	}
	return QVariant(); // Shouldn't happen
}

bool ServerConfig::setConfigString(ConfigKey key, const QString &value)
{
	// Type specific sanity check
	switch(key.type) {
	case ConfigKey::STRING:
	case ConfigKey::BOOL:
		// no type specific validation for these
		break;
	case ConfigKey::SIZE:
		if(parseSizeString(value)<0)
			return false;
		break;
	case ConfigKey::TIME:
		if(parseTimeString(value)<0)
			return false;
		break;
	case ConfigKey::INT: {
		bool ok;
		value.toInt(&ok);
		if(!ok)
			return false;
		break;
		}
	}

	// TODO key specific validation

	setConfigValue(key, value);
	emit configValueChanged(key);
	return true;
}

void ServerConfig::setConfigInt(ConfigKey key, int value)
{
	Q_ASSERT(key.type == ConfigKey::INT || key.type == ConfigKey::SIZE || key.type==ConfigKey::TIME);
	setConfigString(key, QString::number(value));
}

void ServerConfig::setConfigBool(ConfigKey key, bool value)
{
	Q_ASSERT(key.type == ConfigKey::BOOL);
	setConfigString(key, value ? QStringLiteral("true") : QStringLiteral("false"));
}

bool ServerConfig::isAllowedAnnouncementUrl(const QUrl &url) const
{
	Q_UNUSED(url);
	return true;
}

bool ServerConfig::isAddressBanned(const QHostAddress &addr) const
{
	Q_UNUSED(addr);
	return false;
}

RegisteredUser ServerConfig::getUserAccount(const QString &username, const QString &password) const
{
	Q_UNUSED(password);
	return RegisteredUser {
		RegisteredUser::NotFound,
		username,
		QStringList(),
		0
	};
}

int ServerConfig::parseTimeString(const QString &str)
{
	const QRegularExpression re("\\A(\\d+(?:\\.\\d+)?)\\s*([dhms]?)\\z");
	const auto m = re.match(str.toLower());
	if(!m.hasMatch())
		return -1;

	float t = m.captured(1).toFloat();
	if(m.captured(2)=="d")
		t *= 24*60*60;
	else if(m.captured(2)=="h")
		t *= 60*60;
	else if(m.captured(2)=="m")
		t *= 60;

	return t;
}

int ServerConfig::parseSizeString(const QString &str)
{
	const QRegularExpression re("\\A(\\d+(?:\\.\\d+)?)\\s*(gb|mb|kb|b)?\\z");
	const auto m = re.match(str.toLower());
	if(!m.hasMatch())
		return -1;

	float s = m.captured(1).toFloat();
	if(m.captured(2)=="gb")
		s *= 1024*1024*1024;
	else if(m.captured(2)=="mb")
		s *= 1024*1024;
	else if(m.captured(2)=="kb")
		s *= 1024;

	return s;
}

}
