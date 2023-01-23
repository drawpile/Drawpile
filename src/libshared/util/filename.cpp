/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2017 Calle Laakkonen

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

#include "libshared/util/filename.h"

#include <QFileInfo>
#include <QDir>

namespace utils {

QString uniqueFilename(const QDir &dir, const QString &name, const QString &extension, bool fullpath)
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

	if(fullpath)
		return f.absoluteFilePath();
	else
		return f.fileName();
}

QString makeFilenameUnique(const QString &path, const QString &defaultExtension)
{
	Q_ASSERT(defaultExtension.at(0) == '.');

	const QFileInfo f(path);

	if(!f.exists())
		return path;

	const QString name = f.fileName();
	QString base, ext;

	int exti = name.lastIndexOf(defaultExtension, -1, Qt::CaseInsensitive);
	if(exti>0) {
		base = name.left(exti);
		ext = name.mid(exti+1);
	} else {
		base = name;
		ext = defaultExtension.mid(1);
	}

	return uniqueFilename(f.absoluteDir(), base, ext);
}

}
