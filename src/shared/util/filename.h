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

#ifndef UTIL_FILENAME_H
#define UTIL_FILENAME_H

class QString;
class QDir;

namespace utils {

/**
 * @brief Generate a filename that is unique
 *
 * If the file name+extension exist in the directory, a name in
 * format "<name> (n).<extension>" will be generated.
 *
 * @param dir the directory for the file
 * @param name file base name
 * @param extension file extension
 * @return full path to a file that does not exist yet
 */
QString uniqueFilename(const QDir &dir, const QString &name, const QString &extension);

}

#endif
