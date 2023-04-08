// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_UNDO_H
#define DP_NET_UNDO_H

#include "libshared/net/message.h"

namespace protocol {

/**
 * @brief Undo history depth
 *
 * To ensure undo works consistently, each client must store at least this many
 * undo points in their session history, but also limit the functional undo depth
 * to this many points.
 */
static const int UNDO_DEPTH_LIMIT = 30;

/**
 * @brief Undo demarcation point
 *
 * The client sends an UndoPoint message to signal the start of an undoable sequence.
 */
class UndoPoint final : public ZeroLengthMessage<UndoPoint>
{
public:
	UndoPoint(uint8_t ctx) : ZeroLengthMessage(MSG_UNDOPOINT, ctx) {}

	QString messageName() const override { return QStringLiteral("undopoint"); }
};

/**
 * @brief Undo or redo actions
 *
 */
class Undo final : public Message
{
public:
	Undo(uint8_t ctx, uint8_t override, bool redo) : Message(MSG_UNDO, ctx), m_override(override), m_redo(redo) { }

	static Undo *deserialize(uint8_t ctx, const uchar *data, uint len);
	static Undo *fromText(uint8_t ctx, const Kwargs &kwargs, bool redo);

	/**
	 * @brief override user ID
	 *
	 * This is used by session operators to undo actions by other
	 * users. This should be zero when undoing one's own actions.
	 *
	 * @return context id
	 */
	uint8_t overrideId() const { return m_override; }

	/**
	 * @brief Is this a redo operation?
	 */
	bool isRedo() const { return m_redo; }

	QString messageName() const override { return m_redo ? QStringLiteral("redo") : QStringLiteral("undo"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	bool payloadEquals(const Message &m) const override;
	Kwargs kwargs() const override;

private:
	uint8_t m_override;
	uint8_t m_redo;
};

}

#endif
