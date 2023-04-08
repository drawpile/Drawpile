// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DBLOG_H
#define DBLOG_H

#include "libserver/serverlog.h"

#include <QSqlDatabase>

namespace server {

class DbLog final : public ServerLog
{
public:
	explicit DbLog(const QSqlDatabase &db);

	bool initDb();

	QList<Log> getLogEntries(const QString &session, const QDateTime &after, Log::Level atleast, int offset, int limit) const override;

	/**
	 * @brief Delete all log entries older than the given number of days
	 * @param olderThanDays
	 * @return number of entries deleted
	 */
	int purgeLogs(int olderThanDays);

protected:
	void storeMessage(const Log &entry) override;

private:
	QSqlDatabase m_db;
};

}

#endif
