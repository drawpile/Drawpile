// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/inmemoryconfig.h"
#include "libserver/serverlog.h"

namespace server {

InMemoryConfig::InMemoryConfig(QObject *parent)
	: ServerConfig(parent)
	, m_logger(new InMemoryLog)
{
}

InMemoryConfig::~InMemoryConfig()
{
	delete m_logger;
}

QString InMemoryConfig::getConfigValue(const ConfigKey key, bool &found) const
{
	QHash<int, QString>::const_iterator it = m_config.constFind(key.index);
	if(it == m_config.constEnd()) {
		found = false;
		return QString();
	} else {
		found = true;
		return *it;
	}
}

void InMemoryConfig::setConfigValue(ConfigKey key, const QString &value)
{
	m_config[key.index] = value;
}

}
