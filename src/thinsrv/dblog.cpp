// SPDX-License-Identifier: GPL-3.0-or-later
#include "thinsrv/dblog.h"
#include "libshared/util/database.h"
#include <QMetaEnum>
#include <QSqlError>
#include <QSqlQuery>

namespace server {

const QString DbLog::INSERT_SQL =
	QStringLiteral("INSERT INTO serverlog (timestamp, level, topic, user, "
				   "session, message) VALUES (?, ?, ?, ?, ?, ?)");

DbLog::DbLog(const QSqlDatabase &db)
	: m_db(db)
	, m_insertQuery(m_db)
{
}

bool DbLog::initDb()
{
	QSqlQuery q(m_db);
	return utils::db::exec(
		q, QStringLiteral("CREATE TABLE IF NOT EXISTS serverlog ("
						  "timestamp, level, topic, user, session, message)"));
}

QList<Log> DbLog::getLogEntries(
	const QString &session, const QString &user,
	const QString &messageSubstring, const QDateTime &after, Log::Level atleast,
	bool omitSensitive, bool omitKicksAndBans, int offset, int limit) const
{
	QString sql = QStringLiteral(
		"SELECT timestamp, session, user, level, topic, message FROM "
		"serverlog WHERE 1 = 1");
	QVariantList params;
	params.reserve(7);

	if(!session.isEmpty()) {
		sql += QStringLiteral(" AND LOWER(session) = LOWER(?)");
		params.append(session);
	}

	if(!user.isEmpty()) {
		sql += QStringLiteral(" AND INSTR(LOWER(user), LOWER(?))");
		params.append(user);
	}

	if(!messageSubstring.isEmpty()) {
		sql += QStringLiteral(" AND INSTR(LOWER(message), LOWER(?))");
		params.append(messageSubstring);
	}

	if(after.isValid()) {
		sql += QStringLiteral(" AND timestamp >= ?");
		params.append(after.addMSecs(1000).toString(Qt::ISODate));
	}

	if(atleast < Log::Level::Debug) {
		sql += QStringLiteral(" AND level <= ?");
		params.append(int(atleast));
	}

	if(omitSensitive) {
		sql += QStringLiteral(" AND topic <> 'ClientInfo'");
	}

	if(omitKicksAndBans) {
		sql += QStringLiteral(" AND topic NOT IN ('Kick', 'Ban', 'Unban')");
	}

	sql += QStringLiteral(" ORDER BY timestamp DESC, rowid DESC");

	if(limit > 0) {
		sql += QStringLiteral(" LIMIT ?");
		params.append(limit);
	}
	if(offset > 0) {
		sql += QStringLiteral(" OFFSET ?");
		params.append(offset);
	}

	QSqlQuery q(m_db);
	QList<Log> results;
	if(utils::db::exec(q, sql, params)) {
		while(q.next()) {
			results.append(
				Log(q.value(0).toDateTime(), q.value(1).toString(),
					q.value(2).toString(), Log::Level(q.value(3).toInt()),
					Log::Topic(QMetaEnum::fromType<Log::Topic>().keyToValue(
						q.value(4).toString().toUtf8().constData())),
					q.value(5).toString()));
		}
	}
	return results;
}

void DbLog::storeMessage(const Log &entry)
{
	if(!m_insertQueryPrepared) {
		if(utils::db::prepare(m_insertQuery, INSERT_SQL)) {
			m_insertQueryPrepared = true;
		} else {
			return;
		}
	}
	m_insertQuery.bindValue(0, entry.timestamp().toString(Qt::ISODate));
	m_insertQuery.bindValue(1, int(entry.level()));
	m_insertQuery.bindValue(
		2, QMetaEnum::fromType<Log::Topic>().valueToKey(int(entry.topic())));
	m_insertQuery.bindValue(3, entry.user());
	m_insertQuery.bindValue(4, entry.session());
	m_insertQuery.bindValue(5, entry.message());
	if(!utils::db::execPrepared(m_insertQuery, INSERT_SQL)) {
		m_insertQuery.clear();
		m_insertQueryPrepared = false;
	}
}

int DbLog::purgeLogs(int olderThanDays)
{
	if(olderThanDays > 0) {
		QSqlQuery q(m_db);
		if(utils::db::exec(
			   q,
			   QStringLiteral(
				   "DELETE FROM serverlog WHERE timestamp < DATE('now', ?)"),
			   {QStringLiteral("-%1 days").arg(olderThanDays)})) {
			return q.numRowsAffected();
		}
	}
	return 0;
}

}
