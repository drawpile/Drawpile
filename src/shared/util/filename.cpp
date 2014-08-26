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

#include "filename.h"

#include <QFileInfo>

namespace utils {

QString uniqueFilename(const QDir &dir, const QString &name, const QString &extension)
{
	QFileInfo f;
	int n=0;
	do {
		QString fullname;
		if(n==0)
			fullname = name + "." + extension;
		else
			fullname = QStringLiteral("%2 (%1).%3").arg(n).arg(name, extension);

		f.setFile(dir, fullname);
		++n;
	} while(f.exists());

	return f.absoluteFilePath();
}

}
