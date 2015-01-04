/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef DP_RETCON_H
#define DP_RETCON_H

#include "../shared/net/message.h"

#include <QRect>
#include <QList>

namespace drawingboard {

/**
 * @brief Bounds of an operations area of effect
 *
 * This is used to check whether received operations are causally dependent or concurrent with
 * the ones in the local fork.
 */
class AffectedArea
{
public:
	enum Domain {
		USERATTRS, // user attributes: always concurrent with local user's operations
		LAYERATTRS, // layer attributes
		ANNOTATION, // annotations: layer ID is used as the annotation ID
		PIXELS, // layer content: bounding rectangles are checked
		EVERYTHING // fallback
	};

	AffectedArea(Domain domain, int layer, const QRect &bounds=QRect())
		: _domain(domain), _layer(layer), _bounds(bounds) { }

	bool isConcurrentWith(const AffectedArea &other) const;

private:
	Domain _domain;
	int _layer;
	QRect _bounds;
};

}

Q_DECLARE_TYPEINFO(drawingboard::AffectedArea, Q_MOVABLE_TYPE);

namespace drawingboard {

/**
 * @brief Local fork of the session history
 */
class LocalFork
{
public:
	enum MessageAction {
		CONCURRENT, // message was concurrent with local fork: OK to apply
		ALREADYDONE, // message was found at the tip of the local fork
		ROLLBACK // message was not concurrent with the local fork: rollback is needed
	};

	/**
	 * @brief Add a message to the local fork
	 * @param msg
	 * @param area
	 */
	void addLocalMessage(protocol::MessagePtr msg, const AffectedArea &area);

	/**
	 * @brief Handle a message received from the server.
	 *
	 * Possible responses:
	 * - our own message: the message is removed from the local fork
	 * - our own message (out of order): local fork is cleared: need to roll back
	 * - other user's message (concurrent): no need to do anything special
	 * - other user's message (dependent): need to roll back
	 *
	 * When an inconsistency is detected, this function returns ROLLBACK. In that case,
	 * the canvas should be rolled back to some savepoint before the fork, the fork moved
	 * to the end of the session history, and the history then replayed.
	 *
	 * @param msg
	 * @param area
	 * @return message action
	 */
	MessageAction handleReceivedMessage(protocol::MessagePtr msg, const AffectedArea &area);

	/**
	 * @brief Change local fork offset
	 *
	 * This is used when doing a canvas rollback: the local fork is moved to the end of the history
	 *
	 * @param offset new offset
	 */
	void setOffset(int offset);

	/**
	 * @brief Get the fork offset
	 * @return fork's position in the session history
	 */
	int offset() const { return _offset; }

	/**
	 * @brief Are there any messages in the local fork?
	 * @return true if local for is empty
	 */
	bool isEmpty() const { return _messages.isEmpty(); }

	/**
	 * @brief Get the messages in the local fork
	 * @return
	 */
	QList<protocol::MessagePtr> messages() const { return _messages; }

	/**
	 * @brief Empty the local fork
	 */
	void clear();

private:
	QList<protocol::MessagePtr> _messages;
	QList<AffectedArea> _areas;
	int _offset;
};

}

#endif // LOCALFORK_H
