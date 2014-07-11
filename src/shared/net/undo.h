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
#ifndef DP_NET_UNDO_H
#define DP_NET_UNDO_H

#include "message.h"

namespace protocol {

/**
 * @brief The minimum number of undo points clients must store
 *
 * To ensure undo works on all clients, each must keep at least this
 * many undo points in their command history.
 *
 * Note. This number is part of the protocol! Increasing it will break
 * compatibility with previous clients, as they may not store enough
 * history the perform an undo!
 */
static const int UNDO_HISTORY_LIMIT = 30;

/**
 * @brief Undo demarcation point
 *
 * The client sends an UndoPoint message to signal the start of an undoable sequence.
 */
class UndoPoint : public Message
{
public:
	UndoPoint(uint8_t ctx) : Message(MSG_UNDOPOINT, ctx) {}

	static UndoPoint *deserialize(const uchar *data, uint len);
	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;	
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
	 * users. This should be zero when undoing one's own actions.
	 *
	 * @return context id
	 */
	uint8_t overrideId() const { return _override; }

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

	void setPoints(int8_t points) { _points = points; }

	/**
	 * @brief Undo command requires operator privileges if the override field is set
	 * @return true if override field is set
	 */
	bool isOpCommand() const { return _override!=0; }
	bool isUndoable() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;	

private:
	uint8_t _override;
	int8_t _points;
};

}

#endif
