// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_RECORDING_H
#define DP_NET_RECORDING_H

#include "libshared/net/message.h"

#include <QByteArray>
#include <QString>

namespace protocol {

/**
 * @brief Event interval record
 *
 * This is used to preserve timing information in session recordings.
 *
 * Note. The maximum interval (using a single message) is about 65 seconds.
 * Typically the intervals we want to store are a few seconds at most, so this should be enough.
 */
class Interval final : public Message
{
public:
	Interval(uint8_t ctx, uint16_t milliseconds) : Message(MSG_INTERVAL, ctx), m_msecs(milliseconds) {}

	static Interval *deserialize(uint8_t ctx, const uchar *data, uint len);
	static Interval *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t milliseconds() const { return m_msecs; }

	QString messageName() const override { return QStringLiteral("interval"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_msecs;
};

/**
 * @brief A bookmark marker
 *
 * This is used to bookmark points in the session for quick access when playing back a recording.
 */
class Marker final : public Message
{
public:
	Marker(uint8_t ctx, const QString &text) : Message(MSG_MARKER, ctx), m_text(text.toUtf8()) { }

	static Marker *deserialize(uint8_t ctx, const uchar *data, uint len);
	static Marker *fromText(uint8_t ctx, const Kwargs &kwargs);

	QString text() const { return QString::fromUtf8(m_text); }

	QString messageName() const override { return QStringLiteral("marker"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QByteArray m_text;
};

/**
 * @brief A message that has been filtered away (by the ACL filter)
 *
 * Filtered messages are retained in recordings, since they contain
 * valuable debugging information. They are ignored by the client.
 * When written in Text mode, they are written as comments.
 */
class Filtered final : public Message
{
public:
	Filtered(uint8_t ctx, uchar *payload, int payloadLen);
	~Filtered() override;

	static NullableMessageRef deserialize(uint8_t ctx, const uchar *data, uint len);
	// Note: this type has no fromText function since it is serialized as a comment

	/**
	 * @brief Decode the wrapped message.
	 *
	 * Note: it is possible that the wrapped message is invalid. One additional byte
	 * is required to store the type of the wrapped message. If the original payload
	 * length was 65535 bytes, the last byte will be truncated.
	 *
	 * @return Message or nullptr if data is invalid
	 */
	NullableMessageRef decodeWrapped() const;

	/**
	 * @brief Get the type of the wrapped message
	 *
	 * Note: if there is no wrapped message, this returns 0 (MSG_COMMAND).
	 * Command messages should never be filtered anyway, so this is not
	 * a problem.
	 */
	MessageType wrappedType() const { return m_length > 0 ? MessageType(m_payload[0]) : MSG_COMMAND; }

	/**
	 * @brief Get the length of the wrapped message payload
	 *
	 * This does not include the extra byte that indicates the type
	 * of the message.
	 */
	int wrappedPayloadLength() const { return m_length-1; }

	QString messageName() const override { return QStringLiteral("filtered"); }

protected:
	int payloadLength() const override { return m_length; }
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override { return Kwargs(); }
	bool payloadEquals(const Message &m) const override;

private:
	uchar *m_payload;
	int m_length;
};

}

#endif
