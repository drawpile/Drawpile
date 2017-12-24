/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
#ifndef DP_NET_CLIENTINTERNAL_H
#define DP_NET_CLIENTINTERNAL_H

#include "../shared/net/message.h"

namespace protocol {

/**
 * @brief Client internal message
 *
 * Non-serializable pseudomessage used to coordinate processes inside the client.
 */
class ClientInternal : public Message {
public:
	enum class Type {
		Catchup,      // caught up to n% of promised messages
		SequencePoint // message sequence point
	};

	/**
	 * @brief Make a "caught up to n%" message
     *
     * The catchup message causes the StateTracker to emit a caughtUpTo(int) signal.
     * This is used to update a download progress bar.
	 */
	static MessagePtr makeCatchup(int n) { return MessagePtr(new ClientInternal(Type::Catchup, n)); }

	/**
	 * @brief Make a sequence point message
	 *
	 * This message triggers the emission of a sequencePoint(int) signal from the state tracker when it's
	 * reached. It's used by the recording playback controller to synchronize the UI with the
	 * real playback state.
	 */
	static MessagePtr makeSequencePoint(int interval) { return MessagePtr(new ClientInternal(Type::SequencePoint, interval)); }

	Type internalType() const { return m_type; }
	int value() const { return m_value; }

	QString messageName() const override { return QStringLiteral("_internal_"); }

protected:
    int payloadLength() const override { return 0; }
	int serializePayload(uchar*) const override { qWarning("Tried to serialize MSG_INTERNAL"); return 0; }
	bool payloadEquals(const Message &m) const override {
		const ClientInternal &mm = static_cast<const ClientInternal&>(m);
		return m_type == mm.m_type && m_value == mm.m_value;
	}
	Kwargs kwargs() const override { return Kwargs(); }

private:
	ClientInternal(Type type, int value) : Message(MSG_INTERNAL, 0), m_type(type), m_value(value) {}

	Type m_type;
	int m_value;
};

}

#endif

