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

