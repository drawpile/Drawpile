// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/project/metadata.h"

namespace project {

bool retrieveMetadata(
	const QString &dpprPath, const std::function<void(drawdance::Query &)> &fn)
{
	drawdance::Database db;
	bool openOk = db.open(
		dpprPath + QStringLiteral(".meta"),
		QStringLiteral("autosave metadata"));
	if(!openOk) {
		return false;
	}

	{
		drawdance::Query qry = db.query();
		if(!qry.prepare("select value from metadata where name = ?")) {
			return false;
		}
		fn(qry);
	}

	return db.close();
}

bool retrieveMetadataInt(drawdance::Query &qry, const char *name, int &outValue)
{
	if(qry.bind(0, name) && qry.execPrepared() && qry.next()) {
		outValue = qry.columnInt(0);
		return true;
	} else {
		return false;
	}
}

bool retrieveMetadataLongLong(
	drawdance::Query &qry, const char *name, long long &outValue)
{
	if(qry.bind(0, name) && qry.execPrepared() && qry.next()) {
		outValue = qry.columnInt64(0);
		return true;
	} else {
		return false;
	}
}

bool retrieveMetadataString(
	drawdance::Query &qry, const char *name, QString &outValue)
{
	if(qry.bind(0, name) && qry.execPrepared() && qry.next()) {
		outValue = qry.columnText16(0);
		return true;
	} else {
		return false;
	}
}

}
