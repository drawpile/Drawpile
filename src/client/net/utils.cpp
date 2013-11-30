/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include <QImage>

#include "utils.h"
#include "../shared/net/image.h"
#include "../shared/net/pen.h"
#include "core/brush.h"

namespace {
void splitImage(int layer, int x, int y, const QImage &image, bool blend, QList<protocol::MessagePtr> &list)
{
	Q_ASSERT(image.format() == QImage::Format_ARGB32);

	// Compress pixel data and see if it fits in a single message
	QByteArray data(reinterpret_cast<const char*>(image.bits()), image.byteCount());
	QByteArray compressed = qCompress(data);

	if(compressed.length() > protocol::PutImage::MAX_LEN) {
		// Too big! Recursively divide the image and try sending those
		compressed = QByteArray(); // release data
		QImage i1, i2;
		int px, py;
		if(image.width() > image.height()) {
			px = image.width() / 2;
			py = 0;
			i1 = image.copy(0, 0, px, image.height());
			i2 = image.copy(px, 0, image.width()-px, image.height());
		} else {
			px = 0;
			py = image.height() / 2;
			i1 = image.copy(0, 0, image.width(), py);
			i2 = image.copy(0, py, image.width(), image.height()-py);
		}
		splitImage(layer, x, y, i1, blend, list);
		splitImage(layer, x+px, y+py, i2, blend, list);

	} else {
		// It fits! Send data!
		list.append(protocol::MessagePtr(new protocol::PutImage(
			layer,
			blend ? protocol::PutImage::MODE_BLEND : 0,
			x,
			y,
			image.width(),
			image.height(),
			compressed
		)));
	}
}
}

namespace net {

/**
 * Multiple messages are generated if the image is too large to fit in just one.
 * If needed, the image is recursively split into smaller parts.
 * @param layer target layer ID
 * @param x X coordinate
 * @param y Y coordinate
 * @param image the image to put
 * @param blend alpha blend instead of overwrite
 * @return
 */
QList<protocol::MessagePtr> putQImage(int layer, int x, int y, const QImage &image, bool blend)
{
	QList<protocol::MessagePtr> list;
	splitImage(layer, x, y, image.convertToFormat(QImage::Format_ARGB32), blend, list);
	return list;
}

protocol::MessagePtr brushToToolChange(int userid, int layer, const dpcore::Brush &brush)
{
	uint8_t mode = brush.subpixel() ? protocol::TOOL_MODE_SUBPIXEL : 0;
	mode |= brush.incremental() ? protocol::TOOL_MODE_INCREMENTAL : 0;

	return protocol::MessagePtr(new protocol::ToolChange(
		userid,
		layer,
		brush.blendingMode(),
		mode,
		brush.spacing(),
		brush.color(1.0).rgba(),
		brush.color(0.0).rgba(),
		brush.hardness(1.0) * 255,
		brush.hardness(0.0) * 255,
		brush.radius(1.0),
		brush.radius(0.0),
		brush.opacity(1.0) * 255,
		brush.opacity(0.0) * 255
	));
}

}
