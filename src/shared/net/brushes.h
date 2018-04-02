/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "message.h"

#include <cstdint>
#include <QVector>

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
	};

	struct PixelBrushDab {
		int8_t x; // coordinates are relative to the previous db
		int8_t y; // (or origin if this is the first dab)
		uint8_t size;
		uint8_t opacity;

		static const int MAX_XY_DELTA = INT8_MAX;
		static const int LENGTH = 4;
		QString toString() const;
	};
}

Q_DECLARE_TYPEINFO(protocol::ClassicBrushDab, Q_PRIMITIVE_TYPE);
Q_DECLARE_TYPEINFO(protocol::PixelBrushDab, Q_PRIMITIVE_TYPE);

namespace protocol {

typedef QVector<ClassicBrushDab> ClassicBrushDabVector;
typedef QVector<PixelBrushDab> PixelBrushDabVector;

/**
 * @brief Draw Classic Brush Dabs
 *
 */
class DrawDabsClassic : public Message {
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
		: Message(MSG_DRAWDABS_CLASSIC, ctx),
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

	uint16_t layer() const { return m_layer; }
	int32_t originX() const { return m_x; }
	int32_t originY() const { return m_y; }
	uint32_t color() const { return m_color; } // If the alpha channel is set, the dabs are composited indirectly
	uint8_t mode() const { return m_mode; }

	const ClassicBrushDabVector &dabs() const { return m_dabs; }
	ClassicBrushDabVector &dabs() { return m_dabs; }

	QString toString() const override;
	QString messageName() const override { return QStringLiteral("classicdabs"); }

	QRect bounds() const;

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	//bool payloadEquals(const Message &m) const override;
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
class DrawDabsPixel : public Message {
public:
	static const int MAX_DABS = (0xffff - 15) / PixelBrushDab::LENGTH;

	DrawDabsPixel(
		uint8_t ctx,
		uint16_t layer,
		int32_t originX, int32_t originY,
		uint32_t color,
		uint8_t blend,
		const PixelBrushDabVector &dabs=PixelBrushDabVector()
		)
		: Message(MSG_DRAWDABS_PIXEL, ctx),
		m_dabs(dabs),
		m_x(originX), m_y(originY),
		m_color(color),
		m_layer(layer),
		m_mode(blend)
	{
		Q_ASSERT(dabs.size() <= MAX_DABS);
	}

	static DrawDabsPixel *deserialize(uint8_t ctx, const uchar *data, uint len);
	static DrawDabsPixel *fromText(uint8_t ctx, const Kwargs &kwargs, const QStringList &dabs);

	uint16_t layer() const { return m_layer; }
	int32_t originX() const { return m_x; }
	int32_t originY() const { return m_y; }
	uint32_t color() const { return m_color; } // If the alpha channel is set, the dabs are composited indirectly
	uint8_t mode() const { return m_mode; }

	const PixelBrushDabVector &dabs() const { return m_dabs; }
	PixelBrushDabVector &dabs() { return m_dabs; }

	QString toString() const override;
	QString messageName() const override { return QStringLiteral("pixeldabs"); }

	QRect bounds() const;

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	//bool payloadEquals(const Message &m) const override;
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
 * The pen up signals the end of the stroke. In indirect drawing mode, it causes
 * the stroke to be committed to the current layer.
 */
class PenUp : public ZeroLengthMessage<PenUp> {
public:
	PenUp(uint8_t ctx) : ZeroLengthMessage(MSG_PEN_UP, ctx) {}

	QString messageName() const override { return QStringLiteral("penup"); }
};

}

#endif
