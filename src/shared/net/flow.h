/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef DP_NET_FLOWCTRL_H
#define DP_NET_FLOWCTRL_H

#include <QString>

#include "message.h"

namespace protocol {

/**
 * @brief Disconnect notification
 *
 * This message is used when closing the connection gracefully. The message queue
 * will automatically close the socket after sending this message.
 */
class Disconnect : public Message {
public:
	enum Reason {
		ERROR,    // client/server error
		KICK,     // user kicked by session operator
		SHUTDOWN, // client/server closed
		OTHER     // other unspecified error
	};

	Disconnect(Reason reason, const QString &message) : Message(MSG_DISCONNECT, 0),
		_reason(reason), _message(message.toUtf8()) { }

	static Disconnect *deserialize(const uchar *data, uint len);

	/**
	 * Get the reason for the disconnection
	 */
	Reason reason() const { return _reason; }

	/**
	 * Get the disconnect message
	 */
	QString message() const { return QString::fromUtf8(_message); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	Reason _reason;
	QByteArray _message;
};

/**
 * @brief Download progress message
 *
 * The StreamPos message is used to inform the client of roughly how many bytes
 * of data to expect. The client can use this to show a download progress bar.
 */
class StreamPos : public Message {
public:
	StreamPos(uint32_t bytes) : Message(MSG_STREAMPOS, 0),
		_bytes(bytes) {}

	static StreamPos *deserialize(const uchar *data, uint len);

	/**
	 * @brief Number of bytes behind the current stream head
	 * @return byte count
	 */
	uint32_t bytes() const { return _bytes; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint32_t _bytes;
};

}

#endif
