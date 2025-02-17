// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef THINSRV_DBLOG_H
#define THINSRV_DBLOG_H
#include "libserver/serverlog.h"

namespace drawdance {
class Database;
}

namespace server {

class DbLog final : public ServerLog {
public:
	explicit DbLog(drawdance::Database &db);
	~DbLog() override;

	bool initDb();

	QList<Log> getLogEntries(
		const QString &session, const QString &user,
		const QString &messageSubstring, const QDateTime &after,
		Log::Level atleast, bool omitSensitive, bool omitKicksAndBans,
		int offset, int limit) const override;

	/**
	 * @brief Delete all log entries older than the given number of days
	 * @param olderThanDays
	 * @return number of entries deleted
	 */
	int purgeLogs(int olderThanDays);

protected:
	void storeMessage(const Log &entry) override;

private:
	struct Private;
	Private *d;
};

}

#endif
