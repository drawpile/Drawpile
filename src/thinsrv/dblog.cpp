// SPDX-License-Identifier: GPL-3.0-or-later
#include "thinsrv/dblog.h"
#include <QMetaEnum>
#include <dpdb/sql_qt.h>

namespace server {

struct DbLog::Private {
	drawdance::Database &db;
	drawdance::Query query;
};

DbLog::DbLog(drawdance::Database &db)
	: d(new Private{db, db.queryWithoutLock()})
{
}

DbLog::~DbLog()
{
	delete d;
}

bool DbLog::initDb()
{
	return d->query.exec("create table if not exists serverlog ("
						 "timestamp, level, topic, user, session, message)") &&
		   d->query.prepare(
			   "insert into serverlog (timestamp, level, topic, user, "
			   "session, message) values (?, ?, ?, ?, ?, ?)",
			   drawdance::Database::PREPARE_PERSISTENT);
}

QList<Log> DbLog::getLogEntries(
	const QString &session, const QString &user,
	const QString &messageSubstring, const QDateTime &after, Log::Level atleast,
	bool omitSensitive, bool omitKicksAndBans, int offset, int limit) const
{
	QString sql = QStringLiteral(
		"select timestamp, session, user, level, topic, message from "
		"serverlog where 1 = 1");
	QVector<drawdance::Query::Param> params;
	params.reserve(7);

	if(!session.isEmpty()) {
		sql += QStringLiteral(" and lower(session) = lower(?)");
		params.append(session);
	}

	if(!user.isEmpty()) {
		sql += QStringLiteral(" and instr(lower(user), lower(?))");
		params.append(user);
	}

	if(!messageSubstring.isEmpty()) {
		sql += QStringLiteral(" and instr(lower(message), lower(?))");
		params.append(messageSubstring);
	}

	if(after.isValid()) {
		sql += QStringLiteral(" and timestamp >= ?");
		params.append(after.addMSecs(1000).toString(Qt::ISODate));
	}

	if(atleast < Log::Level::Debug) {
		sql += QStringLiteral(" and level <= ?");
		params.append(int(atleast));
	}

	if(omitSensitive) {
		sql += QStringLiteral(" and topic <> 'ClientInfo'");
	}

	if(omitKicksAndBans) {
		sql += QStringLiteral(" and topic not in ('Kick', 'Ban', 'Unban')");
	}

	sql += QStringLiteral(" order by timestamp desc, rowid desc");

	if(limit > 0) {
		sql += QStringLiteral(" limit ?");
		params.append(limit);
	}
	if(offset > 0) {
		sql += QStringLiteral(" offset ?");
		params.append(offset);
	}

	QList<Log> results;
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec(sql, params)) {
		while(query.next()) {
			results.append(Log(
				QDateTime::fromString(query.columnText16(0, true), Qt::ISODate),
				query.columnText16(1), query.columnText16(2),
				Log::Level(query.columnInt(3)),
				Log::Topic(QMetaEnum::fromType<Log::Topic>().keyToValue(
					query.columnText8(4).constData())),
				query.columnText16(5)));
		}
	}
	return results;
}

void DbLog::storeMessage(const Log &entry)
{
	d->query.bind(0, entry.timestamp().toString(Qt::ISODate));
	d->query.bind(1, int(entry.level()));
	d->query.bind(
		2, QMetaEnum::fromType<Log::Topic>().valueToKey(int(entry.topic())));
	d->query.bind(3, entry.user());
	d->query.bind(4, entry.session());
	d->query.bind(5, entry.message());
	d->query.execPrepared();
}

int DbLog::purgeLogs(int olderThanDays)
{
	if(olderThanDays > 0) {
		drawdance::Query query = d->db.queryWithoutLock();
		if(query.exec(
			   "delete from serverlog where timestamp < date('now', ?)",
			   {QStringLiteral("-%1 days").arg(olderThanDays)})) {
			return query.numRowsAffected();
		}
	}
	return 0;
}

}
