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
#ifndef DP_NET_PEN_H
#define DP_NET_PEN_H

#include "message.h"

#include <cstdint>
#include <QVector>

namespace protocol {
	struct PenPoint {
		PenPoint() = default;
		PenPoint(int32_t x_, int32_t y_, uint16_t p_) : x(x_), y(y_), p(p_) {}
		int32_t x, y;
		uint16_t p;

		bool operator!=(const PenPoint &o) const { return x != o.x || y != o.y || p != o.p; }
	};
}

Q_DECLARE_TYPEINFO(protocol::PenPoint, Q_PRIMITIVE_TYPE);

namespace protocol {

static const uint8_t TOOL_MODE_SUBPIXEL = 0x01;
static const uint8_t TOOL_MODE_INCREMENTAL = 0x02;

/**
 * @brief Tool setting change command
 *
 * This command is sent by the client to set the tool properties.
 * The initial state of the tool is undefined by the protocol, so the client
 * must send a ToolChange before sending the first PenMove command. A tool
 * change may only be sent when the client is in a PenUp state.
 */
class ToolChange : public Message {
public:
	ToolChange(
		uint8_t ctx, uint16_t layer,
		uint8_t blend, uint8_t mode, uint8_t spacing,
		uint32_t color,
		uint8_t hard_h, uint8_t hard_l,
		uint8_t size_h, uint8_t size_l,
		uint8_t opacity_h, uint8_t opacity_l,
		uint8_t smudge_h, uint8_t smudge_l,
		uint8_t resmudge
		)
		: Message(MSG_TOOLCHANGE, ctx),
		m_layer(layer), m_blend(blend), m_mode(mode),
		m_spacing(spacing), m_color(color),
		m_hard_h(hard_h), m_hard_l(hard_l), m_size_h(size_h), m_size_l(size_l),
		m_opacity_h(opacity_h), m_opacity_l(opacity_l),
		m_smudge_h(smudge_h), m_smudge_l(smudge_l), m_resmudge(resmudge)
	{}

	static ToolChange *deserialize(uint8_t ctx, const uchar *data, uint len);
	static ToolChange *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const { return m_layer; }
	uint8_t blend() const { return m_blend; }
	uint8_t mode() const { return m_mode; }
	uint8_t spacing() const { return m_spacing; }
	uint32_t color() const { return m_color; }
	uint8_t hard_h() const { return m_hard_h; }
	uint8_t hard_l() const { return m_hard_l; }
	uint8_t size_h() const { return m_size_h; }
	uint8_t size_l() const { return m_size_l; }
	uint8_t opacity_h() const { return m_opacity_h; }
	uint8_t opacity_l() const { return m_opacity_l; }
	uint8_t smudge_h() const { return m_smudge_h; }
	uint8_t smudge_l() const { return m_smudge_l; }
	uint8_t resmudge() const { return m_resmudge; }

	// The client resends ToolChange only if it has changed since
	// the last time it was sent. If the ToolChange is undoable,
	// the effective tool may be different than what the client thinks.
	// To keep things simple, we just don't undo ToolChanges.
	bool isUndoable() const override { return false; }

	QString messageName() const override { return QStringLiteral("brush"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_layer;
	uint8_t m_blend;
	uint8_t m_mode;
	uint8_t m_spacing;
	uint32_t m_color;
	uint8_t m_hard_h;
	uint8_t m_hard_l;
	uint8_t m_size_h;
	uint8_t m_size_l;
	uint8_t m_opacity_h;
	uint8_t m_opacity_l;
	uint8_t m_smudge_h;
	uint8_t m_smudge_l;
	uint8_t m_resmudge;
};

typedef QVector<PenPoint> PenPointVector;

/**
 * @brief Pen move command
 * 
 * The first pen move command starts a new stroke.
 */
class PenMove : public Message {
public:
	//! The maximum number of points that will fit into a single PenMove message
	static const int MAX_POINTS = 0xffff / 10;

	PenMove(uint8_t ctx, const PenPointVector &points)
		: Message(MSG_PEN_MOVE, ctx),
		m_points(points)
	{
		Q_ASSERT(!points.isEmpty());
		Q_ASSERT(points.size() <= MAX_POINTS);
	}
	
	static PenMove *deserialize(uint8_t ctx, const uchar *data, uint len);

	const PenPointVector &points() const { return m_points; }
	PenPointVector &points() { return m_points; }

	QString toString() const override;
	QString messageName() const override { return QStringLiteral("penmove"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override { return Kwargs(); }

private:
	PenPointVector m_points;
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
