/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2008-2017 Calle Laakkonen

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

#include <cstdint>
#include <QString>

#include "libshared/net/message.h"

namespace protocol {

/**
 * @brief A command for creating a new annotation.
 *
 * Annotations are floating text layers. They are drawn over the image layers and
 * have no defined stacking order.
 *
 * The new annotation created with this command is initally empy with a transparent background
 */
class AnnotationCreate final : public Message {
public:
	AnnotationCreate(uint8_t ctx, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_CREATE, ctx), m_id(id), m_x(x), m_y(y), m_w(w), m_h(h)
	{}

	static AnnotationCreate *deserialize(uint8_t ctx, const uchar *data, uint len);
	static AnnotationCreate *fromText(uint8_t ctx, const Kwargs &kwargs);

	/**
	 * @brief The ID of the newly created annotation
	 *
	 * The same rules apply as in layer creation.
	 * @return annotation ID number
	 */
	uint16_t id() const { return m_id; }

	//! Alias for id()
	uint16_t layer() const override { return m_id; }

	int32_t x() const { return m_x; }
	int32_t y() const { return m_y; }
	uint16_t w() const { return m_w; }
	uint16_t h() const { return m_h; }

	/**
	 * @brief Check if the ID's namespace portition matches the context ID
	 * Note. The initial session snapshot may include IDs that do not conform to
	 * the contextId|annotationId format.
	 */
	bool isValidId() const { return (id()>>8) == contextId(); }

	QString messageName() const override { return QStringLiteral("newannotation"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	int32_t m_x;
	int32_t m_y;
	uint16_t m_w;
	uint16_t m_h;
};

/**
 * @brief A command for changing annotation position and size
 */
class AnnotationReshape final : public Message {
public:
	AnnotationReshape(uint8_t ctx, uint16_t id, int32_t x, int32_t y, uint16_t w, uint16_t h)
		: Message(MSG_ANNOTATION_RESHAPE, ctx), m_id(id), m_x(x), m_y(y), m_w(w), m_h(h)
	{}

	static AnnotationReshape *deserialize(uint8_t ctx, const uchar *data, uint len);
	static AnnotationReshape *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t id() const { return m_id; }
	int32_t x() const { return m_x; }
	int32_t y() const { return m_y; }
	uint16_t w() const { return m_w; }
	uint16_t h() const { return m_h; }

	QString messageName() const override { return QStringLiteral("reshapeannotation"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	int32_t m_x;
	int32_t m_y;
	uint16_t m_w;
	uint16_t m_h;
};

/**
 * @brief A command for changing annotation contents
 *
 * Accepted contents is the subset of HTML understood by QTextDocument
 *
 * If an annotation is flagged as protected, it cannot be modified by users
 * other than the one who created it, or session operators.
 */
class AnnotationEdit final : public Message {
public:
	static const uint8_t FLAG_PROTECT = 0x01;       // disallow further modifications from other users
	static const uint8_t FLAG_VALIGN_CENTER = 0x02; // center vertically
	static const uint8_t FLAG_VALIGN_BOTTOM = 0x06; // align to bottom

	AnnotationEdit(uint8_t ctx, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border, const QByteArray &text)
		: Message(MSG_ANNOTATION_EDIT, ctx), m_id(id), m_bg(bg), m_flags(flags), m_border(border), m_text(text)
	{}
	AnnotationEdit(uint8_t ctx, uint16_t id, uint32_t bg, uint8_t flags, uint8_t border, const QString &text)
		: AnnotationEdit(ctx, id, bg, flags, border, text.toUtf8())
	{}

	static AnnotationEdit *deserialize(uint8_t ctx, const uchar *data, uint len);
	static AnnotationEdit *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t id() const { return m_id; }
	uint32_t bg() const { return m_bg; }
	uint8_t flags() const { return m_flags; }
	uint8_t border() const { return m_border; } /* reserved for future use */
	QString text() const { return QString::fromUtf8(m_text); }

	QString messageName() const override { return QStringLiteral("editannotation"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint32_t m_bg;
	uint8_t m_flags;
	uint8_t m_border;
	QByteArray m_text;
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
class AnnotationDelete final : public Message {
public:
	AnnotationDelete(uint8_t ctx, uint16_t id)
		: Message(MSG_ANNOTATION_DELETE, ctx), m_id(id)
	{}

	static AnnotationDelete *deserialize(uint8_t ctx, const uchar *data, uint len);
	static AnnotationDelete *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t id() const { return m_id; }

	QString messageName() const override { return QStringLiteral("deleteannotation"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
};

}

#endif
