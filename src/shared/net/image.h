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
#ifndef DP_NET_IMAGE_H
#define DP_NET_IMAGE_H

#include <QByteArray>
#include <QList>

#include "message.h"

namespace protocol {

class PutImage : public Message {
public:
	static const int MODE_BLEND = (1<<0);
	static const int MAX_LEN = (1<<16) - 10;

	PutImage(uint8_t layer, uint8_t flags, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const QByteArray &image)
	: Message(MSG_PUTIMAGE), _layer(layer), _flags(flags), _x(x), _y(y), _w(w), _h(h), _image(image)
	{}

	static PutImage *deserialize(const uchar *data, uint len);
	
	uint8_t layer() const { return _layer; }
	uint8_t flags() const { return _flags; }
	uint16_t x() const { return _x; }
	uint16_t y() const { return _y; }
	uint16_t width() const { return _w; }
	uint16_t height() const { return _h; }
	const QByteArray &image() const { return _image; }
	
protected:
	/**
	 * \brief Get the length of the message payload
	 * @return payload length in bytes
	 */
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

}

#endif
