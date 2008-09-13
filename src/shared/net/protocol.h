/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#ifndef DP_PROTOCOL_H
#define DP_PROTOCOL_H

#include <QByteArray>

namespace protocol {

/**
 * Packet types recognized by Packet::deserialize.
 */
enum PacketType {
	LOGIN_ID=0,
	MESSAGE,
	BINARY_CHUNK,
	TOOL_SELECT,
	STROKE,
	STROKE_END
};

/**
 * Base class for all message types.
 */
class Packet {
	public:
		/**
		 * Construct a packet.
		 * @param type packet type
		 * @parma len payload length
		 */
		Packet(PacketType type) : _type(type) { }

		virtual ~Packet() { }

		/**
		 * Sniff the length of a packet from the data. Note. data
		 * should be at least 3 bytes long.
		 * @return length of expected packet or 0 if an error was detected.
		 */
		static unsigned int sniffLength(const QByteArray& data);

		/**
		 * Deserialize a packet from raw data.
		 * Check length() to see how many bytes were extracted.
		 * @return new packet or null if data is invalid.
		 */
		static Packet *deserialize(const QByteArray& data);

		/**
		 * Get the type of the packet
		 * @return packet type
		 */
		PacketType type() const { return _type; }

		/**
		 * Get the length of the whole packet
		 * @return payload length in bytes
		 */
		unsigned int length() const { return 3 + payloadLength(); }

		/**
		 * Get the length of the packet payload
		 * @return payload length in bytes
		 */
		virtual unsigned int payloadLength() const = 0;

		/**
		 * Serialize the message.
		 * @return byte array containing length() bytes.
		 */
		QByteArray serialize() const;

	protected:
		/**
		 * Serialize the message payload
		 * @return length of serialized payload in bytes
		 */
		virtual void serializeBody(QIODevice& data) const = 0;

	private:
		const PacketType _type;
};

}

#endif

