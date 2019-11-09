/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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
#ifndef PATHS_H
#define PATHS_H

#include <QStandardPaths>

namespace utils {
namespace paths {

/**
 * @brief Get a list of paths in which datafiles may reside
 */
QStringList dataPaths();

/**
 * @brief Locate a file in dataPaths
 *
 * Returns the first file found or an empty string if not.
 */
QString locateDataFile(const QString &filename);

/**
 * Prepend the path name to the writable location to the given filename.
 *
 * If both dirOrFilename and filename are given, the dirOrFilename should contain a directory component.
 * That directory is created first, if it doesn't exist already.
 */
QString writablePath(QStandardPaths::StandardLocation location, const QString &dirOrFileName, const QString &filename=QString());

inline QString writablePath(const QString &dirOrFileName, const QString &filename=QString())
{
	return writablePath(QStandardPaths::AppDataLocation, dirOrFileName, filename);
}

}
}

#endif
