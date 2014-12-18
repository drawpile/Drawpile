/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
		uint8_t ctx, uint8_t layer,
		uint8_t blend, uint8_t mode, uint8_t spacing,
		uint32_t color_h, uint32_t color_l,
		uint8_t hard_h, uint8_t hard_l,
		uint8_t size_h, uint8_t size_l,
		uint8_t opacity_h, uint8_t opacity_l,
		uint8_t smudge, uint8_t resmudge
		)
		: Message(MSG_TOOLCHANGE, ctx),
		_layer(layer), _blend(blend), _mode(mode),
		_spacing(spacing), _color_h(color_h), _color_l(color_l),
		_hard_h(hard_h), _hard_l(hard_l), _size_h(size_h), _size_l(size_l),
		_opacity_h(opacity_h), _opacity_l(opacity_l),
		_smudge(smudge), _resmudge(resmudge)
		{}

		static ToolChange *deserialize(const uchar *data, uint len);
		
		uint8_t layer() const { return _layer; }
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
		uint8_t smudge() const { return _smudge; }
		uint8_t resmudge() const { return _resmudge; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _layer;
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
	uint8_t _smudge;
	uint8_t _resmudge;
};

struct PenPoint {
	PenPoint() {}
	PenPoint(int32_t x_, int32_t y_, uint16_t p_) : x(x_), y(y_), p(p_) {}
	int32_t x, y;
	uint16_t p;
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

	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

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
};

}

#endif
