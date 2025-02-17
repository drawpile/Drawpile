// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_UTILS_DATABASE_H
#define LIBSHARED_UTILS_DATABASE_H
#include <dpdb/sql_qt.h>

namespace utils {
namespace db {

bool open(
	drawdance::Database &db, const QString &humaneName, const QString &fileName,
	const QString &sourceFileName = QString());

}
}

#endif
