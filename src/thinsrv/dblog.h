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
