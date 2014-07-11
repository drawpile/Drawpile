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

#include "imagesize.h"

#include <QSize>
#include <QDebug>

namespace utils {

//! Check if image dimensions are not too big. Returns true if size is OK
bool checkImageSize(const QSize &size)
{
	// The protocol limits width and height to 2^29 (PenMove, 2^32 for others)
	// However, QPixmap can be at most 32767 pixels wide/tall
	static const int MAX_SIZE = 32767;

	// Rather than limiting the image dimensions to a safe square, allow large rectangular images
	// by limiting size by required memory
	static const quint64 MAX_MEM = 1024 * 1024 * 1024;
	static const quint64 MAX_PIXELS = MAX_MEM / 4;

	qDebug() <<  quint64(size.width())*quint64(size.height()) << MAX_PIXELS;
	return
		size.width() <= MAX_SIZE &&
		size.height() <= MAX_SIZE &&
		quint64(size.width())*quint64(size.height()) <= MAX_PIXELS;
}

}
