// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_STATEDATABASE_H
#define LIBCLIENT_UTILS_STATEDATABASE_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariantList>
#include <functional>

namespace utils {

class StateDatabase final : public QObject {
	Q_OBJECT
public:
	explicit StateDatabase(QObject *parent = nullptr);

	QSqlQuery query() const;

	bool
	exec(QSqlQuery &qry, const QString &sql, const QVariantList &params = {});

	bool prepare(QSqlQuery &qry, QString &sql);
	bool execPrepared(QSqlQuery &qry, const QString &sql);

	bool tx(std::function<bool(QSqlQuery &)> fn);

	QVariant get(const QString &key) const;
	QVariant getWith(QSqlQuery &qry, const QString &key) const;

	bool put(const QString &key, const QVariant &value);
	bool putWith(QSqlQuery &qry, const QString &key, const QVariant &value);

private:
	QSqlDatabase m_db;
};

}

#endif
