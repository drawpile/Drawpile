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
#ifndef NET_COMMANDS_H
#define NET_COMMANDS_H

#include "core/point.h"
#include "core/blendmodes.h"

#include <QJsonArray>
#include <QJsonObject>

namespace protocol {
	class MessagePtr;
	struct PenPoint;
}

namespace paintcore {
	class Brush;
}

class QImage;

namespace net {

//! Convenience functions for constructing various messsages
namespace command {

/**
 * @param Get a ServerCommand
 * @param cmd command name
 * @param args positional arguments
 * @param kwargs keyword arguments
 */
protocol::MessagePtr serverCommand(const QString &cmd, const QJsonArray &args=QJsonArray(), const QJsonObject &kwargs=QJsonObject());

/**
 * @brief Get a "kick user" message
 * @param target ID of the user to kick
 * @param ban ban the user while we're at it?
 */
protocol::MessagePtr kick(int target, bool ban);

/**
 * @brief Get a command to request the removal of the specified IP ban
 * @param entryId
 * @return
 */
protocol::MessagePtr unban(int entryId);

/**
 * @brief Get a "(un)mute user" message
 * @param target ID of the user whose mute flag to change
 * @param mute
 */
protocol::MessagePtr mute(int target, bool mute);

/**
 * @brief Announce this session at the given listing server
 */
protocol::MessagePtr announce(const QString &url);

/**
 * @brief Retract announcement at the given listing server
 */
protocol::MessagePtr unannounce(const QString &url);

/**
 * @brief Generate one or more PutImage command from a QImage
 *
 * Due to the 64k payload length limit, a large image may not fit inside
 * a single message. In this case, the image is recursively split into
 * small enough pieces.
 *
 * If the target coordinates are less than zero, the image is automatically cropped
 *
 * If mode is MODE_REPLACE and the image is large, it is split at tile boundaries (where possible)
 * to generate efficient PutImage commands.
 *
 * Note: when using MODE_REPLACE, the skipempty parameter should be set to false, except when you know
 * the layer is blank.
 */
QList<protocol::MessagePtr> putQImage(int ctxid, int layer, int x, int y, QImage image, paintcore::BlendMode::Mode mode, bool skipempty=true);

//! Generate a tool change message
protocol::MessagePtr brushToToolChange(int ctxid, int layer, const paintcore::Brush &brush);

//! Convert a paintcore point to protocol format
protocol::PenPoint pointToProtocol(const paintcore::Point &p);

//! Generate penMove(s) for multiple points.
QList<protocol::MessagePtr> penMove(int ctxid, const paintcore::PointVector &points);


}
}
#endif
