// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBSHARED_UTILS_DATABASE_H
#define LIBSHARED_UTILS_DATABASE_H

#include <QString>
#include <QVariantList>
#include <functional>

class QSqlDatabase;
class QSqlQuery;

namespace utils {
namespace db {

QSqlDatabase sqlite(
	const QString &connectionName, const QString &humaneName,
	const QString &fileName, const QString &sourceFileName = QString{});

bool prepare(QSqlQuery &query, const QString &sql);

bool bindValue(QSqlQuery &query, int i, const QVariant &value);

bool execPrepared(QSqlQuery &query, const QString &sql);

bool execBatch(QSqlQuery &query, const QString &sql);

bool exec(
	QSqlQuery &query, const QString &sql, const QVariantList &params = {});

bool tx(QSqlDatabase &db, std::function<bool()> fn);

}
}

#endif
