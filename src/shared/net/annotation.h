/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2013 Calle Laakkonen

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
#ifndef DP_NET_ANNOTATION_H
#define DP_NET_ANNOTATION_H

#include <stdint.h>
#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief A command for creating a new annotation.
 *
 * Annotations are floating text layers. They are drawn over the image layers and
 * have no defined stacking order.
 *
 * The new annotation created with this command is initally empy with a transparent background
 */
class AnnotationCreate : public Message {
public:
	AnnotationCreate(uint8_t ctx, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_CREATE, ctx), _id(id), _x(x), _y(y), _w(w), _h(h)
	{}

	static AnnotationCreate *deserialize(uint8_t ctx, const uchar *data, uint len);

	/**
	 * @brief The ID of the newly created annotation
	 *
	 * The same rules apply as in layer creation.
	 * @return annotation ID number
	 */
	uint16_t id() const { return _id; }

	int32_t x() const { return _x; }
	int32_t y() const { return _y; }
	uint16_t w() const { return _w; }
	uint16_t h() const { return _h; }

	/**
	 * @brief Check if the ID's namespace portition matches the context ID
	 * Note. The initial session snapshot may include IDs that do not conform to
	 * the contextId|annotationId format.
	 */
	bool isValidId() const { return (id()>>8) == contextId(); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	int32_t _x;
	int32_t _y;
	uint16_t _w;
	uint16_t _h;
};

/**
 * @brief A command for changing annotation position and size
 */
class AnnotationReshape : public Message {
public:
	AnnotationReshape(uint8_t ctx, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_RESHAPE, ctx), _id(id), _x(x), _y(y), _w(w), _h(h)
	{}

	static AnnotationReshape *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	int32_t x() const { return _x; }
	int32_t y() const { return _y; }
	uint16_t w() const { return _w; }
	uint16_t h() const { return _h; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	int32_t _x;
	int32_t _y;
	uint16_t _w;
	uint16_t _h;
};

/**
 * @brief A command for changing annotation contents
 *
 * Accepted contents is the subset of HTML understood by QTextDocument
 */
class AnnotationEdit : public Message {
public:
	AnnotationEdit(uint8_t ctx, uint16_t id, uint32_t bg, const QByteArray &text)
		: Message(MSG_ANNOTATION_EDIT, ctx), _id(id), _bg(bg), _text(text)
	{}
	AnnotationEdit(uint8_t ctx, uint16_t id, uint32_t bg, const QString &text)
		: AnnotationEdit(ctx, id, bg, text.toUtf8())
	{}

	static AnnotationEdit *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	uint32_t bg() const { return _bg; }
	QString text() const { return QString::fromUtf8(_text); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	uint32_t _bg;
	QByteArray _text;
};

/**
 * @brief A command for deleting an annotation
 *
 * Note. Unlike in layer delete command, there is no "merge" option here.
 * Merging an annotation is done by rendering the annotation item to
 * an image and drawing the image with PutImage command. This is to ensure
 * identical rendering on all clients, as due to font and possible rendering
 * engine differences, text annotations may appear differently on each client.
 */
class AnnotationDelete : public Message {
public:
	AnnotationDelete(uint8_t ctx, uint16_t id)
		: Message(MSG_ANNOTATION_DELETE, ctx), _id(id)
	{}

	static AnnotationDelete *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
};

}

#endif
