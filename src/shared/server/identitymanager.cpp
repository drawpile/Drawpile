/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include "identitymanager.h"
#include "../util/logger.h"

#include <QMetaMethod>

namespace server {

IdentityResult::IdentityResult(QObject *parent)
	: QObject(parent), _status(INPROGRESS)
{}

void IdentityResult::setResults(Status status, const QString &canonicalName, const QStringList &flags)
{
	Q_ASSERT(status != INPROGRESS);
	Q_ASSERT(_status == INPROGRESS);

	_status = status;
	_canonicalName = canonicalName;

	for(const QString &flag : flags) {
		if(flag.compare("mod", Qt::CaseInsensitive)==0)
			_flags << "MOD";
		else if(flag.compare("host", Qt::CaseInsensitive)==0)
			_flags << "HOST";
		else
			logger::warning() << "Unknown user flag:" << flag;
	}

	emit resultAvailable(this);
}

void IdentityResult::connectNotify(const QMetaMethod &signal)
{
	if(signal == QMetaMethod::fromSignal(&IdentityResult::resultAvailable) && _status != INPROGRESS) {
		emit resultAvailable(this);
	}
}

IdentityManager::IdentityManager(QObject *parent) :
	QObject(parent), _needauth(false)
{
}

IdentityResult *IdentityManager::checkLogin(const QString &username, const QString &password)
{
	IdentityResult *result = new IdentityResult(this);
	doCheckLogin(username, password, result);
	return result;
}

}
