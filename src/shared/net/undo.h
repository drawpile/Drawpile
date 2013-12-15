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
#ifndef DP_NET_UNDO_H
#define DP_NET_UNDO_H

#include "message.h"

namespace protocol {

/**
 * @brief Undo demarcation point
 *
 */
class UndoPoint : public Message
{
public:
	UndoPoint(uint8_t ctx) : Message(MSG_UNDOPOINT, ctx) {}

	static UndoPoint *deserialize(const uchar *data, uint len);

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool isUndoable() const { return true; }
};

/**
 * @brief Undo or redo actions
 *
 */
class Undo : public Message
{
public:
	Undo(uint8_t ctx, uint8_t override, int8_t points) : Message(MSG_UNDO, ctx), _override(override), _points(points) { }

	static Undo *deserialize(const uchar *data, uint len);

	/**
	 * @brief override user ID
	 *
	 * This is used by session operators to undo actions by other
	 * users. This is normally set to zero when undoing one's own actions.
	 *
	 * @return context id
	 */
	uint8_t overrideId() const { return _override; }

	/**
	 * @brief Get the effective user ID of this undo command
	 * @return either override ID or normal context ID
	 */
	uint8_t effectiveId() const { return _override ? _override : contextId(); }

	/**
	 * @brief number of actions to undo/redo
	 *
	 * This is the number of undo points the given user's actions should be rolled back.
	 * If the number is negative, the undo points are un-undone.
	 * Zero is not allowed.
	 *
	 * @return points
	 */
	int8_t points() const { return _points; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool isUndoable() const { return true; }

private:
	uint8_t _override;
	int8_t _points;
};

}

#endif
