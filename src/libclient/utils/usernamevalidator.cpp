// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/utils/usernamevalidator.h"
#include "libshared/util/validators.h"

UsernameValidator::UsernameValidator(QObject *parent) :
	QValidator(parent)
{
}

bool UsernameValidator::isValid(const QString &username)
{
	return validateUsername(username);
}

QValidator::State UsernameValidator::validate(QString &input, int &pos) const
{
	Q_UNUSED(pos);

	if(input.isEmpty())
		return Intermediate;

	if(isValid(input))
		return Acceptable;

	return Invalid;
}
