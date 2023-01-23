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
#ifndef INMEMORYCONFIG_H
#define INMEMORYCONFIG_H

#include "libserver/serverconfig.h"

namespace server {

class ServerLog;

class InMemoryConfig : public ServerConfig
{
	Q_OBJECT
public:
	explicit InMemoryConfig(QObject *parent=nullptr);
	~InMemoryConfig();

	ServerLog *logger() const override { return m_logger; }

protected:
	QString getConfigValue(const ConfigKey key, bool &found) const override;
	void setConfigValue(const ConfigKey key, const QString &value) override;

private:
	QHash<int, QString> m_config;
	ServerLog *m_logger;
};

}

#endif
