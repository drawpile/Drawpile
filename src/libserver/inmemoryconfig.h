// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef INMEMORYCONFIG_H
#define INMEMORYCONFIG_H

#include "libserver/serverconfig.h"

namespace server {

class ServerLog;

class InMemoryConfig final : public ServerConfig
{
	Q_OBJECT
public:
	explicit InMemoryConfig(QObject *parent=nullptr);
	~InMemoryConfig() override;

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
