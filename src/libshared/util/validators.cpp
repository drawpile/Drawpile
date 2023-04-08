// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/validators.h"
#include "libshared/util/ulid.h"

#include <QString>
#include <QUuid>

bool validateSessionIdAlias(const QString &alias)
{
	if(alias.length() > 32 || alias.length() < 1)
		return false;

	for(int i=0;i<alias.length();++i) {
		const QChar c = alias.at(i);
		if(!(
			(c >= 'a' && c<='z') ||
			(c >= 'A' && c<='Z') ||
			(c >= '0' && c<='9') ||
			c=='-'
			))
			return false;
	}

	// To avoid confusion with real session IDs,
	// aliases may not be valid UUIDs or ULIDs
	if(!QUuid(alias).isNull())
		return false;

	if(!Ulid(alias).isNull())
		return false;

	return true;
}

bool validateUsername(const QString &username)
{
	if(username.isEmpty())
		return false;

	// Note: the length check is not for any technical limitation, just to
	// make sure people don't use annoyinly long names. QString::length()
	// returns the number of UTF-16 QChars in the string, which might
	// not correspond to the actual number of logical characters, but
	// it should be close enough for this use case.
	if(username.length() > 22)
		return false;

	// Note: there is no technical not to allow this character anymore
	if(username.contains('"'))
		return false;

	return true;
}

