/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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
#ifndef DP_NET_BRUSHES_H
#define DP_NET_BRUSHES_H

#include "libshared/net/message.h"

#include <QVector>
#include <QRect>

class QRect;

namespace protocol {
	struct ClassicBrushDab {
		int8_t x; // coordinates are relative to previous dab
		int8_t y; // (or origin if this is the first dab.)
		uint16_t size; // diameter multiplied by 256
		uint8_t hardness;
		uint8_t opacity;

		static const int MAX_XY_DELTA = INT8_MAX;
		static const int LENGTH = 6;
		QString toString() const;

		bool operator!=(const ClassicBrushDab &o) const {
			return x != o.x || y != o.y || size != o.size || hardness != o.hardness || opacity != o.opacity;
		}
	};

	struct PixelBrushDab {
		int8_t x; // coordinates are relative to the previous dab
		int8_t y; // (or origin if this is the first dab)
		uint8_t size;
		uint8_t opacity;

		static const int MAX_XY_DELTA = INT8_MAX;
		static const int LENGTH = 4;
		QString toString() const;

		bool operator!=(const PixelBrushDab &o) const {
			return x != o.x || y != o.y || size != o.size || opacity != o.opacity;
		}
	};
}

Q_DECLARE_TYPEINFO(protocol::ClassicBrushDab, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(protocol::PixelBrushDab, Q_PRIMITIVE_TYPE);

namespace protocol {

typedef QVector<ClassicBrushDab> ClassicBrushDabVector;
typedef QVector<PixelBrushDab> PixelBrushDabVector;

enum class DabShape {
	Round,
	Square
};

/**
 * @brief Abstract base class for DrawDabs* messages
 */
class DrawDabs : public Message {
public:
	DrawDabs(MessageType type, uint8_t context)
		: Message(type, context)
	{ }

	//! Should these dabs be composited in indirect mode?
	virtual bool isIndirect() const = 0;

	//! Get the last coordinates of the last point in the dab vector
	virtual QPoint lastPoint() const = 0;

	//! Get the bounding rectangle of the dab vector
	virtual QRect bounds() const = 0;

	/**
	 * @brief Append the given dab message's dabs to this message's dab vector.
	 *
	 * If the extension is not possible, this function returns false and
	 * does not modify the current dab vector.
	 *
	 * Possible reasons why extension may fail:
	 * - given DrawDabs instance is of the wrong type
	 * - common properties are not the same
	 * - summed dab array length would be too long
	 * - distance between lastPoint() and dab.originXY is greater than MAX_XY_DELTA
	 */
	virtual bool extend(const DrawDabs &dab) = 0;
};

/**
 * @brief Draw Classic Brush Dabs
 *
 */
class DrawDabsClassic final : public DrawDabs {
public:
	static const int MAX_DABS = (0xffff - 15) / ClassicBrushDab::LENGTH;

	DrawDabsClassic(
		uint8_t ctx,
		uint16_t layer,
		int32_t originX, int32_t originY,
		uint32_t color,
		uint8_t blend,
		const ClassicBrushDabVector &dabs=ClassicBrushDabVector()
		)
		: DrawDabs(MSG_DRAWDABS_CLASSIC, ctx),
		m_dabs(dabs),
		m_x(originX), m_y(originY),
		m_color(color),
		m_layer(layer),
		m_mode(blend)
	{
		Q_ASSERT(dabs.size() <= MAX_DABS);
	}

	static DrawDabsClassic *deserialize(uint8_t ctx, const uchar *data, uint len);
	static DrawDabsClassic *fromText(uint8_t ctx, const Kwargs &kwargs, const QStringList &dabs);

	uint16_t layer() const override { return m_layer; }
	int32_t originX() const { return m_x; } // Classic dab coordinates have subpixel precision.
	int32_t originY() const { return m_y; } // They are converted to integers by multiplying by 4
	uint32_t color() const { return m_color; }
	uint8_t mode() const { return m_mode; }

	// If the color's alpha channel is nonzero, that value is used
	// as the opacity of the entire stroke.
	bool isIndirect() const override { return (m_color & 0xff000000) > 0; }

	const ClassicBrushDabVector &dabs() const { return m_dabs; }
	ClassicBrushDabVector &dabs() { return m_dabs; }

	QString toString() const override;
	QString messageName() const override { return QStringLiteral("classicdabs"); }

	QPoint lastPoint() const override;
	QRect bounds() const override;
	bool extend(const DrawDabs &dab) override;

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override { return Kwargs(); }

private:
	ClassicBrushDabVector m_dabs;
	int32_t m_x, m_y;
	uint32_t m_color;
	uint16_t m_layer;
	uint8_t m_mode;
};


/**
 * @brief Draw Pixel Brush Dabs
 *
 */
class DrawDabsPixel final : public DrawDabs {
public:
	static const int MAX_DABS = (0xffff - 15) / PixelBrushDab::LENGTH;

	DrawDabsPixel(
		DabShape shape,
		uint8_t ctx,
		uint16_t layer,
		int32_t originX, int32_t originY,
		uint32_t color,
		uint8_t blend,
		const PixelBrushDabVector &dabs=PixelBrushDabVector()
		)
		: DrawDabs(shape == DabShape::Square ? MSG_DRAWDABS_PIXEL_SQUARE : MSG_DRAWDABS_PIXEL, ctx),
		m_dabs(dabs),
		m_x(originX), m_y(originY),
		m_color(color),
		m_layer(layer),
		m_mode(blend)
	{
		Q_ASSERT(dabs.size() <= MAX_DABS);
	}

	static DrawDabsPixel *deserialize(DabShape shape, uint8_t ctx, const uchar *data, uint len);
	static DrawDabsPixel *fromText(DabShape shape, uint8_t ctx, const Kwargs &kwargs, const QStringList &dabs);

	uint16_t layer() const override { return m_layer; }
	int32_t originX() const { return m_x; }
	int32_t originY() const { return m_y; }
	uint32_t color() const { return m_color; } // If the alpha channel is set, the dabs are composited indirectly
	uint8_t mode() const { return m_mode; }
	bool isSquare() const { return type() == MSG_DRAWDABS_PIXEL_SQUARE; }

	// If the color's alpha channel is nonzero, that value is used
	// as the opacity of the entire stroke.
	bool isIndirect() const override { return (m_color & 0xff000000) > 0; }

	const PixelBrushDabVector &dabs() const { return m_dabs; }
	PixelBrushDabVector &dabs() { return m_dabs; }

	QString toString() const override;
	QString messageName() const override { return isSquare() ? QStringLiteral("squarepixeldabs") : QStringLiteral("pixeldabs"); }

	QPoint lastPoint() const override;
	QRect bounds() const override;
	bool extend(const DrawDabs &dab) override;

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override { return Kwargs(); }

private:
	PixelBrushDabVector m_dabs;
	int32_t m_x, m_y;
	uint32_t m_color;
	uint16_t m_layer;
	uint8_t m_mode;
};

/**
 * @brief Pen up command
 *
 * The pen up command signals the end of a stroke. In indirect drawing mode, it causes
 * indirect dabs (by this user) to be merged to their parent layers.
 */
class PenUp : public ZeroLengthMessage<PenUp> {
public:
	PenUp(uint8_t ctx) : ZeroLengthMessage(MSG_PEN_UP, ctx) {}

	QString messageName() const override { return QStringLiteral("penup"); }
};

}

#endif
