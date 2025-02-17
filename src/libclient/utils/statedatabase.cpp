// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/statedatabase.h"
#include "libshared/util/database.h"

namespace utils {

StateDatabase::StateDatabase(QObject *parent)
	: QObject(parent)
{
	if(utils::db::open(
		   m_db, QStringLiteral("state"), QStringLiteral("state.db"))) {
		drawdance::Query qry = m_db.query();
		qry.setForeignKeysEnabled(false);
		qry.exec("create table if not exists migrations ("
				 "migration_id integer primary key not null)");
		qry.exec("create table if not exists state ("
				 "key text primary key not null,"
				 "value)");
		executeMigrations(qry);
		qry.setForeignKeysEnabled(true);
	}
}

bool StateDatabase::getBool(const QString &key, bool defaultValue)
{
	drawdance::Query qry = query();
	return getBoolWith(qry, key, defaultValue);
}

QString
StateDatabase::getString(const QString &key, const QString &defaultValue)
{
	drawdance::Query qry = query();
	return getStringWith(qry, key, defaultValue);
}

bool StateDatabase::put(
	const QString &key, const drawdance::Query::Param &value)
{
	drawdance::Query qry = query();
	return putWith(qry, key, value);
}

bool StateDatabase::remove(const QString &key)
{
	drawdance::Query qry = query();
	return removeWith(qry, key);
}

bool StateDatabase::getBoolWith(
	drawdance::Query &qry, const QString &key, bool defaultValue) const
{
	return getWith(qry, key) ? qry.columnBool(0) : defaultValue;
}

QString StateDatabase::getStringWith(
	drawdance::Query &qry, const QString &key,
	const QString &defaultValue) const
{
	return getWith(qry, key) ? qry.columnText16(0) : defaultValue;
}

bool StateDatabase::putWith(
	drawdance::Query &qry, const QString &key,
	const drawdance::Query::Param &value)
{
	return qry.exec(
		"insert into state (key, value) values (?1, ?2) "
		"on conflict (key) do update set value = ?2",
		{key, value});
}

bool StateDatabase::removeWith(drawdance::Query &qry, const QString &key)
{
	return qry.exec("delete from state where key = ?", {key});
}

void StateDatabase::executeMigrations(drawdance::Query &qry)
{
	QVector<std::function<bool(StateDatabase *, drawdance::Query &)>>
		migrations = {
			&StateDatabase::executeMigration1HostPresets,
		};

	int migrationCount = migrations.size();
	const char *selectMigrationsSql = "select migration_id from migrations "
									  "order by migration_id desc limit 1";
	const char *insertMigrationsSql =
		"insert into migrations (migration_id) values (?)";

	if(qry.exec(selectMigrationsSql)) {
		int lastMigrationId = qry.next() ? qry.columnInt(0) : 0;
		for(int i = lastMigrationId; i < migrationCount; ++i) {
			bool ok = qry.tx([&] {
				return migrations[i](this, qry) &&
					   qry.exec(insertMigrationsSql, {i + 1});
			});
			if(!ok) {
				qWarning("Migration %d failed", i);
				break;
			}
		}
	}
}

bool StateDatabase::executeMigration1HostPresets(drawdance::Query &qry)
{
	return qry.exec("create table host_presets ("
					"id integer primary key not null,"
					"version integer not null,"
					"title text not null,"
					"data blob not null)");
}

bool StateDatabase::getWith(drawdance::Query &qry, const QString &key) const
{
	return qry.exec("select value from state where key = ?", {key}) &&
		   qry.next();
}

}
