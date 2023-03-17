/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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
#ifndef DP_NET_OPAQUE_H
#define DP_NET_OPAQUE_H

#include "libshared/net/message.h"

#include <QByteArray>

namespace protocol {

/**
 * @brief An opaque message
 *
 * This is treated as opaque binary data by the server. The client needs to be able
 * to decode these, though.
 *
 */
class OpaqueMessage final : public Message
{
public:
	OpaqueMessage(MessageType type, uint8_t ctx, const uchar *payload, int payloadLen);
	~OpaqueMessage() override;
	OpaqueMessage(const OpaqueMessage &m) = delete;
	OpaqueMessage &operator=(const OpaqueMessage &m) = delete;

	static NullableMessageRef decode(MessageType type, uint8_t ctx, const uchar *data, uint len);

	/**
	 * @brief Decode this message
	 * @return Message or nullptr if data is invalid
	 */
	NullableMessageRef decode() const;

	QString messageName() const override { return QStringLiteral("_opaque"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override { return Kwargs(); }

private:
	uchar *m_payload;
	int m_length;
};

}

#endif
