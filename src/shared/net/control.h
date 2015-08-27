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
#ifndef DP_NET_CTRL_H
#define DP_NET_CTRL_H

#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief Server command message
 *
 * This is a general purpose message for sending commands to the server
 * and receiving replies. This is used for (among other things):
 * - the login handshake
 * - setting session parameters (e.g. max user count and password)
 * - sending administration commands (e.g. kick user)
 */
class Command : public Message {
public:
	Command(uint8_t ctx, const QByteArray &msg) : Message(MSG_COMMAND, ctx), m_msg(msg) {}
	Command(uint8_t ctx, const QString &msg) : Command(ctx, msg.toUtf8()) {}

	static Command *deserialize(uint8_t ctxid, const uchar *data, uint len);

	QString message() const { return QString::fromUtf8(m_msg); }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QByteArray m_msg;
};

}

#endif
