// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_STATEDATABASE_H
#define LIBCLIENT_UTILS_STATEDATABASE_H
#include <QObject>
#include <dpdb/sql_qt.h>

namespace utils {

class StateDatabase final : public QObject {
	Q_OBJECT
public:
	explicit StateDatabase(QObject *parent = nullptr);

	// Acquires a recursive mutex on the state database, releases when the query
	// goes out of scope. Don't let it deadlock in multiple threads.
	drawdance::Query query() { return m_db.query(); }

	// Acquires a recursive mutex on the state database, releases when the
	// transaction is over. Don't let it deadlock in multiple threads.
	bool tx(std::function<bool(drawdance::Query &)> block)
	{
		return m_db.tx(block);
	}

	// These all also acquire and release a mutex on the state database. Use the
	// *With variants below when you already acquired a query beforehand.
	bool getBool(const QString &key, bool defaultValue);
	QString getString(const QString &key, const QString &defaultValue);
	bool put(const QString &key, const drawdance::Query::Param &value);
	bool remove(const QString &key);

	bool getBoolWith(
		drawdance::Query &qry, const QString &key, bool defaultValue) const;
	QString getStringWith(
		drawdance::Query &qry, const QString &key,
		const QString &defaultValue) const;
	bool putWith(
		drawdance::Query &qry, const QString &key,
		const drawdance::Query::Param &value);
	static bool removeWith(drawdance::Query &qry, const QString &key);

private:
	void executeMigrations(drawdance::Query &qry);
	bool executeMigration1HostPresets(drawdance::Query &qry);
	bool getWith(drawdance::Query &qry, const QString &key) const;

	drawdance::Database m_db;
};

}

#endif
