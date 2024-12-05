// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_STATEDATABASE_H
#define LIBCLIENT_UTILS_STATEDATABASE_H
#include <QObject>
#include <QRecursiveMutex>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <functional>

namespace utils {

class StateDatabase final : public QObject {
	Q_OBJECT
public:
	class Query {
		friend class StateDatabase;

	public:
		~Query();
		Query(const Query &) = delete;
		Query(Query &&) = default;
		Query &operator=(const Query &) = delete;
		Query &operator=(Query &&) = delete;

		bool exec(const QString &sql, const QVariantList &params = {});
		bool prepare(const QString &sql);
		bool execPrepared();
		void bindValue(int pos, const QVariant &val);
		bool next();
		QVariant value(int i) const;
		int numRowsAffected() const;

		QVariant get(const QString &key);
		bool put(const QString &key, const QVariant &value);
		bool remove(const QString &key);

	private:
		QRecursiveMutex &m_mutex;
		QSqlQuery m_query;
		QString m_preparedSql;

		Query(QRecursiveMutex &mutex, const QSqlDatabase &db);
	};

	explicit StateDatabase(QObject *parent = nullptr);

	// Acquires a recursive mutex on the state database, releases when the query
	// goes out of scope. Don't let it deadlock in multiple threads.
	Query query() const;

	// Acquires a recursive mutex on the state database, releases when the
	// transaction is over. Don't let it deadlock in multiple threads.
	bool tx(std::function<bool(Query &)> fn);

	// These all also acquire and release a mutex on the state database. Use
	// Query::(get|put|remove) when dealing with transactions or nested scopes.
	QVariant get(const QString &key) const;
	bool put(const QString &key, const QVariant &value);
	bool remove(const QString &key);

private:
	void executeMigrations(QSqlQuery &qry);
	bool executeMigration1HostPresets(QSqlQuery &qry);

	mutable QRecursiveMutex m_mutex;
	QSqlDatabase m_db;
};

}

#endif
