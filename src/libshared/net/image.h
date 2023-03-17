/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2018 Calle Laakkonen

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

#include "libshared/net/message.h"

#include <QByteArray>
#include <QList>
#include <QRect>

namespace protocol {

/**
 * @brief Draw a bitmap onto a layer
 *
 * This is used when initializing the canvas from an existing file
 * and when pasting images.
 *
 * All brush/layer blending modes are supported.
 *
 * The image data is DEFLATEd 32bit premultiplied ARGB data.
 *
 * Note that since the message length is fairly limited, a
 * large image may have to be divided into multiple PutImage
 * commands.
 */
class PutImage final : public Message {
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
	
	uint16_t layer() const override { return m_layer; }
	uint8_t blendmode() const { return m_mode; }
	uint32_t x() const { return m_x; }
	uint32_t y() const { return m_y; }
	uint32_t width() const { return m_w; }
	uint32_t height() const { return m_h; }
	const QByteArray &image() const { return m_image; }

	QString messageName() const override { return QStringLiteral("putimage"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
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
 * @brief Set the content of a tile
 *
 * Unlike PutImage, this replaces an entire tile directly without any blending.
 * This command is typically used during canvas initialization to set the initial content.
 *
 * PutTiles can be targeted at sublayers as well. This is used when generating a reset image
 * with incomplete indirect strokes. Sending a PenUp command will merge the sublayer.
 */
class PutTile final : public Message {
public:
	/**
	 * @brief Construct a solid color PutTile
	 * @param ctx context ID
	 * @param layer target layer
	 * @param sublayer sublayer (0 means no sublayer)
	 * @param col tile column
	 * @param row tile row
	 * @param repeat put this many extra tiles
	 * @param color tile fill color ARGB (unpremultiplied)
	 */
	PutTile(uint8_t ctx, uint16_t layer, uint8_t sublayer, uint16_t col, uint16_t row, uint16_t repeat, uint32_t color);

	/**
	 * @brief Construct a PutTile
	 * @param ctx context ID
	 * @param layer target layer
	 * @param sublayer sublayer (0 means no sublayer)
	 * @param col tile column
	 * @param row tile row
	 * @param repeat put this many extra tiles
	 * @param image tile content. Uncompressed length must be 64x64x4
	 */
	PutTile(uint8_t ctx, uint16_t layer, uint8_t sublayer, uint16_t col, uint16_t row, uint16_t repeat, const QByteArray &image)
	: Message(MSG_PUTTILE, ctx), m_layer(layer), m_col(col), m_row(row), m_repeat(repeat), m_sublayer(sublayer), m_image(image)
	{
		// Note: an uncompressed tile is only 16KB, so this should never be
		// anywhere near this long
		Q_ASSERT(image.length() <= 0xffff - 8);
		Q_ASSERT(image.length() >= 4);
	}

	static PutTile *deserialize(uint8_t ctx, const uchar *data, uint len);
	static PutTile *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_layer; }
	uint8_t sublayer() const { return m_sublayer; }
	uint16_t column() const { return m_col; }
	uint16_t row() const { return m_row; }
	uint16_t repeat() const { return m_repeat; }
	uint32_t color() const;
	const QByteArray &image() const { return m_image; }

	bool isSolidColor() const { return m_image.length() == 4; }

	QString messageName() const override { return QStringLiteral("puttile"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_layer;
	uint16_t m_col;
	uint16_t m_row;
	uint16_t m_repeat;
	uint8_t m_sublayer;
	QByteArray m_image;
};

/**
 * @brief Set the canvas background
 *
 */
class CanvasBackground final : public Message {
public:
	/**
	 * @brief Construct a solid color background
	 * @param ctx context ID
	 * @param color background color ARGB (unpremultiplied)
	 */
	CanvasBackground(uint8_t ctx, uint32_t color);

	/**
	 * @brief Construct a pattern background
	 * @param ctx context ID
	 * @param image tile content. Uncompressed length must be 64x64x4
	 */
	CanvasBackground(uint8_t ctx, const QByteArray &image)
	: Message(MSG_CANVAS_BACKGROUND, ctx), m_image(image)
	{
		// Note: an uncompressed tile is only 16KB, so this should never be
		// anywhere near this long
		Q_ASSERT(image.length() <= 0xffff - 8);
		Q_ASSERT(image.length() >= 4);
	}

	static CanvasBackground *deserialize(uint8_t ctx, const uchar *data, uint len);
	static CanvasBackground *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint32_t color() const;
	const QByteArray &image() const { return m_image; }

	bool isSolidColor() const { return m_image.length() == 4; }

	QString messageName() const override { return QStringLiteral("background"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override;

private:
	QByteArray m_image;
};


/**
 * @brief Fill a rectangle with solid color
 *
 * All brush blending modes are supported
 */
class FillRect final : public Message {
public:
	FillRect(uint8_t ctx, uint16_t layer, uint8_t blend, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
		: Message(MSG_FILLRECT, ctx), m_layer(layer), m_blend(blend), m_x(x), m_y(y), m_w(w), m_h(h), m_color(color)
	{
	}

	static FillRect *deserialize(uint8_t ctx, const uchar *data, uint len);
	static FillRect *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_layer; }
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

/**
 * @brief Move (and transform) a region of a layer.
 *
 * This is used to implement selection moving. It is equivalent
 * to doing two PutImages: the first to mask away the original
 * selection and the other to paste the selection to a new location.
 *
 * This command packages that into a single action that is more
 * bandwidth efficient and can be used even when PutImages in general
 * are locked, since it's not introducing any new pixels onto the canvas.
 *
 * Internally, the paint engine performs the following steps:
 * 1. Copy selected pixels to a buffer
 * 2. Erase selected pixels from the layer
 * 3. Composite transformed buffer onto the layer
 *
 * The pixel selection is determined by the mask bitmap. The mask
 * is DEFLATEd 1 bit per pixel bitmap data.
 * For axis aligned rectangle selections, no bitmap is necessary.
 */
class MoveRegion final : public Message {
public:
	//! Maximum length of the mask (it's compressed 1bpp image data, typically representing a simple polygon. ~64k should be more than plenty)
	static const int MAX_LEN = 0xffff - 50;

	/**
	 * @brief Construct a MoveRegion message
	 * @param ctx context ID
	 * @param layer layer ID
	 * @param bx source bounding rect X
	 * @param by source bounding rect Y
	 * @param bw source bounding rect width
	 * @param bh source bounding rect height
	 * @param x1 target quad vertex 1 X
	 * @param y1 target quad vertex 1 Y
	 * @param x2 target quad vertex 2 X
	 * @param y2 target quad vertex 2 Y
	 * @param x3 target quad vertex 3 X
	 * @param y3 target quad vertex 3 Y
	 * @param x4 target quad vertex 4 X
	 * @param y4 target quad vertex 4 Y
	 * @param mask source mask bitmap
	 */
	MoveRegion(uint8_t ctx, uint16_t layer, int32_t bx, int32_t by, int32_t bw, int32_t bh, int32_t x1, int32_t y1, int32_t x2, int32_t y2, int32_t x3, int32_t y3, int32_t x4, int32_t y4, const QByteArray &mask)
		: Message(MSG_REGION_MOVE, ctx), m_layer(layer), m_bx(bx), m_by(by), m_bw(bw), m_bh(bh),
		  m_x1(x1), m_y1(y1), m_x2(x2), m_y2(y2), m_x3(x3), m_y3(y3), m_x4(x4), m_y4(y4),
		  m_mask(mask)
	{ }

	static MoveRegion *deserialize(uint8_t ctx, const uchar *data, uint len);
	static MoveRegion *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_layer; }
	int32_t bx() const { return m_bx; }
	int32_t by() const { return m_by; }
	int32_t bw() const { return m_bw; }
	int32_t bh() const { return m_bh; }

	int32_t x1() const { return m_x1; }
	int32_t y1() const { return m_y1; }

	int32_t x2() const { return m_x2; }
	int32_t y2() const { return m_y2; }

	int32_t x3() const { return m_x3; }
	int32_t y3() const { return m_y3; }

	int32_t x4() const { return m_x4; }
	int32_t y4() const { return m_y4; }

	QByteArray mask() const { return m_mask; }

	QString messageName() const override { return QStringLiteral("moveregion"); }

	QRect sourceBounds() const { return QRect(bx(), by(), bw(), bh()); }
	QRect targetBounds() const {
		const int left = qMin(qMin(x1(), x2()), qMin(x3(), x4()));
		const int right = qMax(qMax(x1(), x2()), qMax(x3(), x4()));
		const int top = qMin(qMin(y1(), y2()), qMin(y3(), y4()));
		const int bottom = qMax(qMax(y1(), y2()), qMax(y3(), y4()));
		return QRect(QPoint(left, top), QPoint(right, bottom));
	}

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_layer;
	int32_t m_bx, m_by, m_bw, m_bh;
	int32_t m_x1, m_y1, m_x2, m_y2, m_x3, m_y3, m_x4, m_y4;
	QByteArray m_mask;
};

}

#endif
