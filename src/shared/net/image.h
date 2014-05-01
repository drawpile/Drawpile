/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_NET_IMAGE_H
#define DP_NET_IMAGE_H

#include <QByteArray>
#include <QList>

#include "message.h"

namespace protocol {

/**
 * @brief Draw a bitmap onto a layer
 *
 * This is used when initializing the canvas from an existing file
 * and when pasting images.
 *
 * If the BLEND flag is set, the image is alpha-blended onto the canvas. Otherwise
 * the new pixels will simply overwrite the existing ones, alpha values and all.
 *
 * The image data is DEFLATEd 32bit non-premultiplied ARGB data.
 *
 * The contextId doesn't affect the way the bitmap is
 * drawn, but it is needed to identify the user so PutImages
 * can be undone/redone.
 *
 * Note that since the message length is fairly limited, a
 * large image may have to be divided into multiple PutImage
 * commands.
 */
class PutImage : public Message {
public:
	static const int MODE_BLEND = (1<<0);

	//! Maximum length of image data array
	static const int MAX_LEN = (1<<16) - 11;

	PutImage(uint8_t ctx, uint8_t layer, uint8_t flags, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const QByteArray &image)
	: Message(MSG_PUTIMAGE, ctx), _layer(layer), _flags(flags), _x(x), _y(y), _w(w), _h(h), _image(image)
	{
		Q_ASSERT(image.length() <= MAX_LEN);
	}

	static PutImage *deserialize(const uchar *data, uint len);
	
	uint8_t layer() const { return _layer; }
	uint8_t flags() const { return _flags; }
	uint16_t x() const { return _x; }
	uint16_t y() const { return _y; }
	uint16_t width() const { return _w; }
	uint16_t height() const { return _h; }
	const QByteArray &image() const { return _image; }

	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _layer;
	uint8_t _flags;
	uint16_t _x;
	uint16_t _y;
	uint16_t _w;
	uint16_t _h;
	QByteArray _image;
};

/**
 * @brief Fill a rectangle with solid color
 *
 * The rectangle is blended onto the layer using the normal blending operations.
 * The special mode 255 is used to indicate that no blending should be used and the
 * new color should just overwrite existing layer content.
 */
class FillRect : public Message {
public:
	FillRect(uint8_t ctx, uint8_t layer, uint8_t blend, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint32_t color)
		: Message(MSG_FILLRECT, ctx), _layer(layer), _blend(blend), _x(x), _y(y), _w(w), _h(h), _color(color)
	{
	}

	static FillRect *deserialize(const uchar *data, uint len);

	uint8_t layer() const { return _layer; }
	uint8_t blend() const { return _blend; }
	uint16_t x() const { return _x; }
	uint16_t y() const { return _y; }
	uint16_t width() const { return _w; }
	uint16_t height() const { return _h; }
	uint32_t color() const { return _color; }

	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _layer;
	uint8_t _blend;
	uint16_t _x;
	uint16_t _y;
	uint16_t _w;
	uint16_t _h;
	uint32_t _color;
};

}

#endif
