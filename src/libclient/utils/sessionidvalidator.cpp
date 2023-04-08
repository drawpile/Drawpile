// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/sessionidvalidator.h"
#include "libshared/util/validators.h"

SessionIdAliasValidator::SessionIdAliasValidator(QObject *parent) :
	QValidator(parent)
{
}

QValidator::State SessionIdAliasValidator::validate(QString &input, int &pos) const
{
	Q_UNUSED(pos);

	if(input.isEmpty() || validateSessionIdAlias(input))
		return Acceptable;
	else
		return Invalid;
}
