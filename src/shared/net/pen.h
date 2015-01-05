/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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

#include <cstdint>
#include <QVector>

#include "message.h"

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

static const uint8_t TOOL_MODE_SUBPIXEL = (1<<0);
static const uint8_t TOOL_MODE_INCREMENTAL = (1<<1);

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
		uint32_t color_h, uint32_t color_l,
		uint8_t hard_h, uint8_t hard_l,
		uint8_t size_h, uint8_t size_l,
		uint8_t opacity_h, uint8_t opacity_l,
		uint8_t smudge_h, uint8_t smudge_l,
		uint8_t resmudge
		)
		: Message(MSG_TOOLCHANGE, ctx),
		_layer(layer), _blend(blend), _mode(mode),
		_spacing(spacing), _color_h(color_h), _color_l(color_l),
		_hard_h(hard_h), _hard_l(hard_l), _size_h(size_h), _size_l(size_l),
		_opacity_h(opacity_h), _opacity_l(opacity_l),
		_smudge_h(smudge_h), _smudge_l(smudge_l), _resmudge(resmudge)
		{}

		static ToolChange *deserialize(const uchar *data, uint len);
		
		uint16_t layer() const { return _layer; }
		uint8_t blend() const { return _blend; }
		uint8_t mode() const { return _mode; }
		uint8_t spacing() const { return _spacing; }
		uint32_t color_h() const { return _color_h; }
		uint32_t color_l() const { return _color_l; }
		uint8_t hard_h() const { return _hard_h; }
		uint8_t hard_l() const { return _hard_l; }
		uint8_t size_h() const { return _size_h; }
		uint8_t size_l() const { return _size_l; }
		uint8_t opacity_h() const { return _opacity_h; }
		uint8_t opacity_l() const { return _opacity_l; }
		uint8_t smudge_h() const { return _smudge_h; }
		uint8_t smudge_l() const { return _smudge_l; }
		uint8_t resmudge() const { return _resmudge; }

	// The client resends ToolChange only if it has changed since
	// the last time it was sent. If the ToolChange is undoable,
	// the effective tool may be different than what the client thinks.
	// To keep things simple, we just don't undo ToolChanges.
	bool isUndoable() const { return false; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _layer;
	uint8_t _blend;
	uint8_t _mode;
	uint8_t _spacing;
	uint32_t _color_h;
	uint32_t _color_l;
	uint8_t _hard_h;
	uint8_t _hard_l;
	uint8_t _size_h;
	uint8_t _size_l;
	uint8_t _opacity_h;
	uint8_t _opacity_l;
	uint8_t _smudge_h;
	uint8_t _smudge_l;
	uint8_t _resmudge;
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
	static const int MAX_POINTS = (0xffff-1) / 10;

	PenMove(uint8_t ctx, const PenPointVector &points)
		: Message(MSG_PEN_MOVE, ctx),
		_points(points)
	{
		Q_ASSERT(points.size() <= MAX_POINTS);
	}
	
	static PenMove *deserialize(const uchar *data, uint len);

	const PenPointVector &points() const { return _points; }
	PenPointVector &points() { return _points; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool payloadEquals(const Message &m) const;

private:
	PenPointVector _points;
};

/**
 * @brief Pen up command
 *
 * The pen up signals the end of the stroke. In indirect drawing mode, it causes
 * the stroke to be committed to the current layer.
 */
class PenUp : public Message {
public:
	PenUp(uint8_t ctx) : Message(MSG_PEN_UP, ctx) {}
	
	static PenUp *deserialize(const uchar *data, uint len);

	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool payloadEquals(const Message &m) const;
};

}

#endif
