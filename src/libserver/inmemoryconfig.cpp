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

#include "libserver/inmemoryconfig.h"
#include "libserver/serverlog.h"

namespace server {

InMemoryConfig::InMemoryConfig(QObject *parent)
	: ServerConfig(parent), m_logger(new InMemoryLog)
{
}

InMemoryConfig::~InMemoryConfig()
{
	delete m_logger;
}

QString InMemoryConfig::getConfigValue(const ConfigKey key, bool &found) const
{
	if(m_config.count(key.index)==0) {
		found = false;
		return QString();
	} else {
		found = true;
		return m_config[key.index];
	}
}

void InMemoryConfig::setConfigValue(ConfigKey key, const QString &value)
{
	m_config[key.index] = value;
}

}
