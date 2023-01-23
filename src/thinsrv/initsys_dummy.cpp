/*
   Drawpile - a collaborative drawing program.

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

#include "thinsrv/initsys.h"

#include <cstdio>

namespace initsys {

void notifyReady()
{
	// dummy
}

void notifyStatus(const QString &status)
{
#ifndef NDEBUG
	fprintf(stderr, "STATUS: %s\n", status.toLocal8Bit().constData());
#else
	Q_UNUSED(status);
#endif
}

QList<int> getListenFds()
{
	return QList<int>();
}

}
