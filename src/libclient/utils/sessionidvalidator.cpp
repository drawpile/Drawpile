/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

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
