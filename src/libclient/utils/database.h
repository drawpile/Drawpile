// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef LIBCLIENT_UTILS_DATABASE_H
#define LIBCLIENT_UTILS_DATABASE_H

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

bool execPrepared(QSqlQuery &query, const QString &sql);

bool exec(
	QSqlQuery &query, const QString &sql, const QVariantList &params = {});

bool tx(QSqlDatabase &db, std::function<bool()> fn);

}
}

#endif
