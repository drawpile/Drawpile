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

#ifndef UPDATEABLEFILE_H
#define UPDATEABLEFILE_H

#include <QDateTime>
#include <QSharedPointer>

class QFile;

/**
 * @brief A wrapper for (configuration) files that must be reloaded when changed
 */
class UpdateableFile
{
public:
	UpdateableFile();
	explicit UpdateableFile(const QString &path);

	void setPath(const QString &path);
	QString path() const { return _path; }

	/**
	 * @brief Check if the file has been modified since it was last opened
	 * @return
	 */
	bool isModified() const;

	/**
	 * @brief Open the file
	 *
	 * After calling this, isModified() will return true until the file
	 * has been changed again.
	 *
	 * @return
	 */
	QSharedPointer<QFile> open();

private:
	QString _path;
	QDateTime _lastmod;
};

#endif // UPDATEABLEFILE_H
