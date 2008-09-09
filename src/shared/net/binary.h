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
		BinaryChunk(QByteArray data) : Packet(BINARY_CHUNK), _data(data) { }

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

