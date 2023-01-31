/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#ifndef IMAGESIZECHECK_H
#define IMAGESIZECHECK_H

class QSize;
class QImage;
class QColor;

#include <QVector>
#include <QPair>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size);

/**
 * @brief Check if we support writing an image file with this format
 *
 * The the format is identified from the filename suffix.
 * Note: only formats supported by the Rustpile library are included
 * in the list.
 */
bool isWritableFormat(const QString &filename);

enum FileFormatOption {
	Images = 0x01,
	Recordings = 0x02,
	AllFiles = 0x04,
	Save = 0x08,
	QtImagesOnly = 0x10,  // return images supported by Qt, rather than Rustpile
	Profile = 0x20,
	DebugDumps = 0x40,
	EventLog = 0x80,

	OpenImages = Images | AllFiles,
	OpenEverything = Images | Recordings | AllFiles,
	OpenDebugDumps = DebugDumps,
	SaveImages = Images | AllFiles | Save,
	SaveRecordings = Recordings | AllFiles | Save,
	SaveProfile = Profile | Save,
	SaveEventLog = EventLog | Save,
};
Q_DECLARE_FLAGS(FileFormatOptions, FileFormatOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormatOptions)

//! Get a filter string to use in an Open or Save dialog
QString fileFormatFilter(FileFormatOptions formats);

}

#endif

