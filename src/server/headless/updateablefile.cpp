/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "updateablefile.h"
#include "../shared/util/logger.h"

#include <QFileInfo>
#include <QFile>

UpdateableFile::UpdateableFile()
{

}

UpdateableFile::UpdateableFile(const QString &path)
{
	setPath(path);
}

void UpdateableFile::setPath(const QString &path)
{
	_path = path;
	_lastmod = QDateTime();
}

bool UpdateableFile::isModified() const
{
	Q_ASSERT(!_path.isEmpty());
	QFileInfo f(_path);
	if(!f.exists())
		return false;

	return f.lastModified() != _lastmod;
}

QSharedPointer<QFile> UpdateableFile::open()
{
	QSharedPointer<QFile> f(new QFile(_path));
	if(!f->open(QFile::ReadOnly))
		logger::error() << _path << "Error:" << f->errorString();
	else
		_lastmod = QFileInfo(*f).lastModified();

	return f;
}
