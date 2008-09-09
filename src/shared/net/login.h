#ifndef DP_PROTO_LOGIN_H
#define DP_PROTO_LOGIN_H

#include "protocol.h"

namespace protocol {

/**
 * A login identifier message. This is the first message sent when a connection
 * is made.
 */
class LoginId : public Packet {
	public:
		/**
		 * Construct a login id message. The protocol revision number is
		 * used to filter out incompatible clients. The software version number
		 * is changed when clientside incompatibilities are introduced.
		 * @param magic a 4 byte magic number.
		 * @param rev protocol revision.
		 * @param ver software version.
		 */
		LoginId(const char magic[4], int rev, int ver);

		/**
		 * Construct a login id message using the default magic and revision
		 * numbers.
		 * @param ver software version.
		 */
		LoginId(int ver);

		/**
		 * Deserialize a login message.
		 */
		static LoginId *deserialize(QIODevice& data, int len);

		/**
		 * Length of the login message payload.
		 */
		unsigned int payloadLength() const { return 8; }

		/**
		 * Get the magic number of the message.
		 */
		const char *magicNumber() const { return _magic; }

		/**
		 * Get the protocol revision number.
		 */
		int protocolRevision() const { return _rev; }

		/**
		 * Get the software version number.
		 */
		int softwareVersion() const { return _ver; }

		/**
		 * Check if the magic number and protocol revision match
		 * this protocol.
		 */
		bool isCompatible() const;

	protected:
		void serializeBody(QIODevice& data) const;

	private:
		char _magic[4];
		const int _rev, _ver;
};

}

#endif

