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

#include "utils/archive.h"

#include <KArchive>
#include <QDebug>

namespace utils {

QByteArray getArchiveFile(const KArchive &archive, const QString &filename, qint64 maxlen)
{
	const KArchiveEntry *e = archive.directory()->entry(filename);
	if(!e || !e->isFile())
		return QByteArray();

	const KArchiveFile *ef = static_cast<const KArchiveFile*>(e);

	if(ef->size()>maxlen) {
		qWarning() << "archive file" << filename << "too long:" << ef->size() << "maximum is" << maxlen;
		return QByteArray();
	}

	return ef->data();
}

}
