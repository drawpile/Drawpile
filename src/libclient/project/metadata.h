// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_PROJECT_METADATARETRIEVER_H
#define LIBCLIENT_PROJECT_METADATARETRIEVER_H
#include <functional>
#include <libshared/util/database.h>

namespace project {

bool retrieveMetadata(
	const QString &dpprPath, const std::function<void(drawdance::Query &)> &fn);

bool retrieveMetadataInt(
	drawdance::Query &qry, const char *name, int &outValue);

bool retrieveMetadataLongLong(
	drawdance::Query &qry, const char *name, long long &outValue);

bool retrieveMetadataString(
	drawdance::Query &qry, const char *name, QString &outValue);

}

#endif
