/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "../shared/net/message.h"

#include <QVector>
#include <QPair>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size);

/**
 * @brief Check if we support writing an image file with this format
 *
 * The the format is identified from the filename suffix.
 * Note that this function may return true even if the format is not found
 * in the writableImageFormats list, since that list is restricted to just
 * a reasonable set of commonly used formats. 
 *
 * @param filename
 * @return
 */
bool isWritableFormat(const QString &filename);

//! Get a whitelisted set of writable image formats
QVector<QPair<QString,QByteArray>> writableImageFormats();

enum FileFormatOption {
	Images = 0x01,
	Recordings = 0x02,
	AllFiles = 0x04,
	Save = 0x08,
	QtImagesOnly = 0x10,

	OpenImages = Images | AllFiles,
	OpenEverything = Images | Recordings | AllFiles,
	SaveImages = Images | AllFiles | Save,
	SaveRecordings = Recordings | AllFiles | Save
};
Q_DECLARE_FLAGS(FileFormatOptions, FileFormatOption)
Q_DECLARE_OPERATORS_FOR_FLAGS(FileFormatOptions)

//! Get a filter string to use in an Open or Save dialog
QString fileFormatFilter(FileFormatOptions formats);

QColor isSolidColorImage(const QImage &image);

}

#endif

