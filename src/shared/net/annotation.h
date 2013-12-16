/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#ifndef DP_NET_ANNOTATION_H
#define DP_NET_ANNOTATION_H

#include <stdint.h>
#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief A command for creating a new annotation.
 *
 * The new annotation is initally empy with a transparent background
 */
class AnnotationCreate : public Message {
public:
	AnnotationCreate(uint8_t ctx, uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_CREATE, ctx), _id(id), _x(x), _y(y), _w(w), _h(h)
	{}

	static AnnotationCreate *deserialize(const uchar *data, uint len);

	/**
	 * @brief The ID of the newly created annotation
	 *
	 * The same rules apply as in layer creation.
	 * @return annotation ID number
	 */
	uint8_t id() const { return _id; }

	void setId(uint8_t id) { _id = id; }

	uint16_t x() const { return _x; }
	uint16_t y() const { return _y; }
	uint16_t w() const { return _w; }
	uint16_t h() const { return _h; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint16_t _x;
	uint16_t _y;
	uint16_t _w;
	uint16_t _h;
};

/**
 * @brief A command for changing annotation position and size
 */
class AnnotationReshape : public Message {
public:
	AnnotationReshape(uint8_t ctx, uint8_t id, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_RESHAPE, ctx), _id(id), _x(x), _y(y), _w(w), _h(h)
	{}

	static AnnotationReshape *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	uint16_t x() const { return _x; }
	uint16_t y() const { return _y; }
	uint16_t w() const { return _w; }
	uint16_t h() const { return _h; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint16_t _x;
	uint16_t _y;
	uint16_t _w;
	uint16_t _h;
};

/**
 * @brief A command for changing annotation contents
 */
class AnnotationEdit : public Message {
public:
	AnnotationEdit(uint8_t ctx, uint8_t id, uint32_t bg, const QByteArray &text)
		: Message(MSG_ANNOTATION_EDIT, ctx), _id(id), _bg(bg), _text(text)
	{}
	AnnotationEdit(uint8_t ctx, uint8_t id, uint32_t bg, const QString &text)
		: AnnotationEdit(ctx, id, bg, text.toUtf8())
	{}

	static AnnotationEdit *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	uint32_t bg() const { return _bg; }
	QString text() const { return QString::fromUtf8(_text); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint32_t _bg;
	QByteArray _text;
};

/**
 * @brief A command for deleting an annotation
 */
class AnnotationDelete : public Message {
public:
	AnnotationDelete(uint8_t ctx, uint8_t id)
		: Message(MSG_ANNOTATION_DELETE, ctx), _id(id)
	{}

	static AnnotationDelete *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool isUndoable() const { return true; }

private:
	uint8_t _id;
};

}

#endif
