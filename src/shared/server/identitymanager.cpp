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

#include <QtConcurrent>

namespace server {

IdentityManager::IdentityManager(QObject *parent) :
	QObject(parent), _needauth(false)
{
}

QFuture<IdentityResult> IdentityManager::checkLogin(const QString &username, const QString &password)
{
	return QtConcurrent::run([this, username, password]() { return doCheckLogin(username, password); });
}

}
