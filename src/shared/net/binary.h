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

#ifndef DP_PROTO_BINARY_H
#define DP_PROTO_BINARY_H

#include <QByteArray>

#include "protocol.h"

namespace protocol {

/**
 * A chunk of binary data.
 */
class BinaryChunk : public Packet {
	public:
		/**
		 * Construct a message containing a chunk of binary data.
		 * Maximum length of the chunk is 2^16 - 3 bytes.
		 */
		explicit BinaryChunk(QByteArray data) : Packet(BINARY_CHUNK), _data(data) { }

		/**
		 * Deserialize a binary data chunk
		 */
		static BinaryChunk *deserialize(QIODevice& data, int len) {
			return new BinaryChunk(data.read(len));
		}

		/**
		 * Get the data in this message.
		 */
		const QByteArray& data() const { return _data; }

		/**
		 * Get the length of the message data
		 */
		unsigned int payloadLength() const { return _data.length(); }

	protected:
		void serializeBody(QIODevice& data) const {
			data.write(_data);
		}

	private:
		QByteArray _data;
};

}

#endif

