/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2017 Calle Laakkonen

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
 * All brush/layer blending modes are supported.
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
	//! Maximum length of image data array
	static const int MAX_LEN = 0xffff - 19;

	PutImage(uint8_t ctx, uint16_t layer, uint8_t mode, uint32_t x, uint32_t y, uint32_t w, uint32_t h, const QByteArray &image)
	: Message(MSG_PUTIMAGE, ctx), m_layer(layer), m_mode(mode), m_x(x), m_y(y), m_w(w), m_h(h), m_image(image)
	{
		Q_ASSERT(image.length() <= MAX_LEN);
	}

	static PutImage *deserialize(uint8_t ctx, const uchar *data, uint len);
	static PutImage *fromText(uint8_t ctx, const Kwargs &kwargs);
	
	uint16_t layer() const { return m_layer; }
	uint8_t blendmode() const { return m_mode; }
	uint32_t x() const { return m_x; }
	uint32_t y() const { return m_y; }
	uint32_t width() const { return m_w; }
	uint32_t height() const { return m_h; }
	const QByteArray &image() const { return m_image; }

	QString messageName() const override { return QStringLiteral("putimage"); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool payloadEquals(const Message &m) const;
	Kwargs kwargs() const override;

private:
	uint16_t m_layer;
	uint8_t m_mode;
	uint32_t m_x;
	uint32_t m_y;
	uint32_t m_w;
	uint32_t m_h;
	QByteArray m_image;
};

/**
 * @brief Fill a rectangle with solid color
 *
 * All brush blending modes are supported
 */
class FillRect : public Message {
public:
	FillRect(uint8_t ctx, uint16_t layer, uint8_t blend, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
		: Message(MSG_FILLRECT, ctx), m_layer(layer), m_blend(blend), m_x(x), m_y(y), m_w(w), m_h(h), m_color(color)
	{
	}

	static FillRect *deserialize(uint8_t ctx, const uchar *data, uint len);
	static FillRect *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const { return m_layer; }
	uint8_t blend() const { return m_blend; }
	uint32_t x() const { return m_x; }
	uint32_t y() const { return m_y; }
	uint32_t width() const { return m_w; }
	uint32_t height() const { return m_h; }
	uint32_t color() const { return m_color; }

	QString messageName() const override { return QStringLiteral("fillrect"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_layer;
	uint8_t m_blend;
	uint32_t m_x;
	uint32_t m_y;
	uint32_t m_w;
	uint32_t m_h;
	uint32_t m_color;
};

}

#endif
