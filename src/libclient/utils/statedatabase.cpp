// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/statedatabase.h"
#include "libshared/util/database.h"
#include <QMutexLocker>
#include <QSqlError>

namespace utils {

StateDatabase::Query::~Query()
{
	m_mutex.unlock();
}

bool StateDatabase::Query::exec(const QString &sql, const QVariantList &params)
{
	return db::exec(m_query, sql, params);
}

bool StateDatabase::Query::prepare(const QString &sql)
{
	m_preparedSql = sql;
	return db::prepare(m_query, sql);
}

bool StateDatabase::Query::execPrepared()
{
	return db::execPrepared(m_query, m_preparedSql);
}

void StateDatabase::Query::bindValue(int pos, const QVariant &val)
{
	m_query.bindValue(pos, val);
}

bool StateDatabase::Query::next()
{
	return m_query.next();
}

QVariant StateDatabase::Query::value(int i) const
{
	return m_query.value(i);
}

int StateDatabase::Query::numRowsAffected() const
{
	return m_query.numRowsAffected();
}

QVariant StateDatabase::Query::get(const QString &key)
{
	bool hasRow =
		exec("select value from state where key = ?", {key}) && next();
	return hasRow ? value(0) : QVariant{};
}

bool StateDatabase::Query::put(const QString &key, const QVariant &value)
{
	QString sql =
		QStringLiteral("insert into state (key, value) values (?, ?)\n"
					   "	on conflict (key) do update set value = ?");
	return exec(sql, {key, value, value});
}

bool StateDatabase::Query::remove(const QString &key)
{
	QString sql = QStringLiteral("delete from state where key = ?");
	return exec(sql, {key}) && numRowsAffected() > 0;
}

StateDatabase::Query::Query(QRecursiveMutex &mutex, const QSqlDatabase &db)
	: m_mutex(mutex)
	, m_query(db)
{
}


StateDatabase::StateDatabase(QObject *parent)
	: QObject(parent)
	, m_db(utils::db::sqlite(
		  QStringLiteral("drawpile_state_connection"), QStringLiteral("state"),
		  QStringLiteral("state.db")))
{
	QSqlQuery qry(m_db);
	utils::db::exec(qry, "pragma foreign_keys = on");
	utils::db::exec(
		qry, "create table if not exists migrations (\n"
			 "	migration_id integer primary key not null)");
	utils::db::exec(
		qry, "create table if not exists state (\n"
			 "	key text primary key not null,\n"
			 "	value)");
	executeMigrations(qry);
}


StateDatabase::Query StateDatabase::query() const
{
	m_mutex.lock();
	return Query(m_mutex, m_db);
}

bool StateDatabase::tx(std::function<bool(Query &)> fn)
{
	Query qry = query();
	return db::tx(m_db, [&] {
		return fn(qry);
	});
}

QVariant StateDatabase::get(const QString &key) const
{
	return query().get(key);
}

bool StateDatabase::put(const QString &key, const QVariant &value)
{
	return query().put(key, value);
}

bool StateDatabase::remove(const QString &key)
{
	return query().remove(key);
}

void StateDatabase::executeMigrations(QSqlQuery &qry)
{
	QVector<std::function<bool(StateDatabase *, QSqlQuery &)>> migrations = {
		&StateDatabase::executeMigration1HostPresets,
	};

	int migrationCount = migrations.size();
	QString selectMigrationsSql =
		QStringLiteral("select migration_id from migrations\n"
					   "order by migration_id desc limit 1");
	QString insertMigrationsSql =
		QStringLiteral("insert into migrations (migration_id) values (?)");

	if(utils::db::exec(qry, selectMigrationsSql)) {
		int lastMigrationId = qry.next() ? qry.value(0).toInt() : 0;
		for(int i = lastMigrationId; i < migrationCount; ++i) {
			bool ok = db::tx(m_db, [&] {
				return migrations[i](this, qry) &&
					   db::exec(qry, insertMigrationsSql, {i + 1});
			});
			if(!ok) {
				qWarning("Migration %d failed", i);
				break;
			}
		}
	}
}

bool StateDatabase::executeMigration1HostPresets(QSqlQuery &qry)
{
	return db::exec(
		qry, QStringLiteral("create table host_presets (\n"
							"	id integer primary key not null,\n"
							"	version integer not null,\n"
							"	title text not null,\n"
							"	data blob not null)"));
}

}
