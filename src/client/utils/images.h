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

#ifndef IMAGESIZECHECK_H
#define IMAGESIZECHECK_H

class QSize;
class QImage;
class QColor;

#include "../shared/net/message.h"

#include <QList>
#include <QPair>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size);

/**
 * @brief Check if we support writing an image file with this format
 *
 * The the format is identified from the filename suffix.
 *
 * @param filename
 * @return
 */
bool isWritableFormat(const QString &filename);

//! Get a whitelisted set of writable image formats
QList<QPair<QString,QByteArray>> writableImageFormats();

//! Get a list of recording formats for use in a file dialog
QString recordingFormatFilter(bool allFiles=true);

QColor isSolidColorImage(const QImage &image);

}

#endif

