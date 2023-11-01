// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/statedatabase.h"
#include "libshared/util/database.h"
#include <QMutex>
#include <QMutexLocker>
#include <QSqlError>
#include <QSqlQuery>

namespace utils {

StateDatabase::StateDatabase(QObject *parent)
	: QObject{parent}
	, m_db{utils::db::sqlite(
		  QStringLiteral("drawpile_state_connection"), QStringLiteral("state"),
		  QStringLiteral("state.db"))}
{
	QSqlQuery qry{query()};
	utils::db::exec(qry, "pragma foreign_keys = on");
	utils::db::exec(
		qry, "create table if not exists migrations (\n"
			 "	migration_id integer primary key not null)");
	utils::db::exec(
		qry, "create table if not exists state (\n"
			 "	key text primary key not null,\n"
			 "	value)");
}

QSqlQuery StateDatabase::query() const
{
	return QSqlQuery{m_db};
}

bool StateDatabase::exec(
	QSqlQuery &qry, const QString &sql, const QVariantList &params)
{
	return db::exec(qry, sql, params);
}

bool StateDatabase::prepare(QSqlQuery &qry, QString &sql)
{
	return db::prepare(qry, sql);
}

bool StateDatabase::execPrepared(QSqlQuery &qry, const QString &sql)
{
	return db::execPrepared(qry, sql);
}

bool StateDatabase::tx(std::function<bool(QSqlQuery &)> fn)
{
	return db::tx(m_db, [&] {
		QSqlQuery qry = query();
		return fn(qry);
	});
}

QVariant StateDatabase::get(const QString &key) const
{
	QSqlQuery qry = query();
	return getWith(qry, key);
}

QVariant StateDatabase::getWith(QSqlQuery &qry, const QString &key) const
{
	bool hasRow =
		utils::db::exec(qry, "select value from state where key = ?", {key}) &&
		qry.next();
	return hasRow ? qry.value(0) : QVariant{};
}

bool StateDatabase::put(const QString &key, const QVariant &value)
{
	QSqlQuery qry = query();
	return putWith(qry, key, value);
}

bool StateDatabase::putWith(
	QSqlQuery &qry, const QString &key, const QVariant &value)
{
	QString sql =
		QStringLiteral("insert into state (key, value) values (?, ?)\n"
					   "	on conflict (key) do update set value = ?");
	return utils::db::exec(qry, sql, {key, value, value});
}

bool StateDatabase::remove(const QString &key)
{
	QSqlQuery qry = query();
	return removeWith(qry, key);
}

bool StateDatabase::removeWith(QSqlQuery &qry, const QString &key)
{
	QString sql = QStringLiteral("delete from state where key = ?");
	return utils::db::exec(qry, sql, {key}) && qry.numRowsAffected() > 0;
}

}
