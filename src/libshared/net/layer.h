// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_NET_LAYER_H
#define DP_NET_LAYER_H

#include <cstdint>
#include <QString>
#include <QList>

#include "libshared/net/message.h"

namespace protocol {

/**
 * \brief Canvas size adjustment command
 *
 * This is the first command that must be sent to initialize the session.
 *
 * This affects the size of all existing and future layers.
 *
 * The new canvas size is relative to the old one. The four adjustement
 * parameters extend or rectract their respective borders.
 * Initial canvas resize should be (0, w, h, 0).
 */
class CanvasResize final : public Message {
public:
	CanvasResize(uint8_t ctx, int32_t top, int32_t right, int32_t bottom, int32_t left)
		: Message(MSG_CANVAS_RESIZE, ctx), m_top(top), m_right(right), m_bottom(bottom), m_left(left)
		{}

	static CanvasResize *deserialize(uint8_t ctx, const uchar *data, uint len);
	static CanvasResize *fromText(uint8_t ctx, const Kwargs &kwargs);

	int32_t top() const { return m_top; }
	int32_t right() const { return m_right; }
	int32_t bottom() const { return m_bottom; }
	int32_t left() const { return m_left; }

	QString messageName() const override { return QStringLiteral("resize"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	int32_t m_top;
	int32_t m_right;
	int32_t m_bottom;
	int32_t m_left;
};

/**
 * \brief Layer creation command.
 *
 * A session starts with zero layers, so a layer creation command is typically
 * the second command to be sent, right after setting the canvas size.
 *
 * The layer ID must be prefixed with the context ID of the user creating it.
 * This allows users to choose the layer ID themselves without worrying about
 * clashes. In single user mode, the client can assign IDs as it pleases,
 * but in multiuser mode the server validates the prefix for all new layers.
 *
 * The following flags can be used with layer creation:
 * - COPY   -- a copy of the Source layer is made, rather than a blank layer
 * - INSERT -- the new layer is inserted above the Source layer. Source 0 means
 *             the layer will be placed bottom-most on the stack
 *
 * The Source layer ID should be zero when COPY or INSERT flags are not used.
 * When COPY is used, it should refer to an existing layer. Copy commands
 * referring to missing layers are dropped.
 * When INSERT is used, referring to 0 or a nonexistent layer places
 * the new layer at the bottom of the stack.
 *
 * If layer controls are locked, this command requires session operator privileges.
 */
class LayerCreate final : public Message {
public:
	static const uint8_t FLAG_COPY = 0x01;
	static const uint8_t FLAG_INSERT = 0x02;

	LayerCreate(uint8_t ctxid, uint16_t id, uint16_t source, uint32_t fill, uint8_t flags, const QString &title)
		: Message(MSG_LAYER_CREATE, ctxid), m_id(id), m_source(source), m_fill(fill), m_flags(flags), m_title(title.toUtf8())
		{}

	static LayerCreate *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerCreate *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	uint16_t source() const { return m_source; }
	uint32_t fill() const { return m_fill; }
	uint8_t flags() const { return m_flags; }
	QString title() const { return QString::fromUtf8(m_title); }

	/**
	 * @brief Check if the ID's namespace portition matches the context ID
	 *
	 * Note. This check is only needed during normal multiuser operation. Layers
	 * created in single-user mode can use any ID.
	 * This means layer IDs of the initial snapshot need not be validated.
	 */
	bool isValidId() const { return (m_id>>8) == contextId(); }

	QString messageName() const override { return QStringLiteral("newlayer"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint16_t m_source;
	uint32_t m_fill;
	uint8_t m_flags;
	QByteArray m_title;
};

/**
 * @brief Layer attribute change command
 *
 * If the current layer or layer controls in general are locked, this command
 * requires session operator privileges.
 *
 * Specifying a sublayer requires session operator privileges. Currently, it is used
 * only when sublayers are needed at canvas initialization.
 */
class LayerAttributes final : public Message {
public:
	static const uint8_t FLAG_CENSOR = 0x01; // censored layer
	static const uint8_t FLAG_FIXED  = 0x02; // fixed background/foreground layer (drawn even in solo modo)

	LayerAttributes(uint8_t ctx, uint16_t id, uint8_t sublayer, uint8_t flags, uint8_t opacity, uint8_t blend)
		: Message(MSG_LAYER_ATTR, ctx), m_id(id),
		m_sublayer(sublayer), m_flags(flags), m_opacity(opacity), m_blend(blend)
		{}

	static LayerAttributes *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerAttributes *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	uint8_t sublayer() const { return m_sublayer; }
	uint8_t flags() const { return m_flags; }
	uint8_t opacity() const { return m_opacity; }
	uint8_t blend() const { return m_blend; }

	bool isCensored() const { return m_flags & FLAG_CENSOR; }
	bool isFixed() const { return m_flags & FLAG_FIXED; }

	QString messageName() const override { return QStringLiteral("layerattr"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint8_t m_sublayer;
	uint8_t m_flags;
	uint8_t m_opacity;
	uint8_t m_blend;
};

/**
 * @brief Layer visibility (visible/hidden) change command
 *
 * This command is used to toggle the layer visibility for the local user.
 * (I.e. any user is allowed to send this command and it has no effect on
 * other users.)
 * Even though this only affects the sending user, this message can be
 * sent through the official session history to keep the architecture simple.
 *
 * Note: to hide the layer for all users, use LayerAttributes to set its opacity
 * to zero.
 */
class LayerVisibility final : public Message {
public:
	LayerVisibility(uint8_t ctx, uint16_t id, uint8_t visible)
		: Message(MSG_LAYER_VISIBILITY, ctx), m_id(id), m_visible(visible)
	{ }

	static LayerVisibility *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerVisibility *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	uint8_t visible() const { return m_visible; }

	QString messageName() const override { return QStringLiteral("layervisibility"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint8_t m_visible;
};

/**
 * @brief Layer title change command
 *
 * If the current layer or layer controls in general are locked, this command
 * requires session operator privileges.
 */
class LayerRetitle final : public Message {
public:
	LayerRetitle(uint8_t ctx, uint16_t id, const QByteArray &title)
		: Message(MSG_LAYER_RETITLE, ctx), m_id(id), m_title(title)
		{}
	LayerRetitle(uint8_t ctx, uint16_t id, const QString &title)
		: LayerRetitle(ctx, id, title.toUtf8())
		{}

	static LayerRetitle *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerRetitle *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	QString title() const { return QString::fromUtf8(m_title); }

	QString messageName() const override { return QStringLiteral("retitlelayer"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	QByteArray m_title;
};

/**
 * @brief Layer order change command
 *
 * New layers are always added to the top of the stack.
 * This command includes a list of layer IDs that define the new stacking order.
 *
 * An order change should list all layers in the stack, but due to synchronization issues, that
 * is not always possible.
 * The layer order should therefore be sanitized by removing all layers not in the current layer stack
 * and adding all missing layers to the end in their current relative order.
 *
 * For example: if the current stack is [1,2,3,4,5] and the client receives
 * a reordering command [3,4,1], the missing layers are appended: [3,4,1,2,5].
 *
 * If layer controls are locked, this command requires session operator privileges.
 */
class LayerOrder final : public Message {
public:
	LayerOrder(uint8_t ctx, const QList<uint16_t> &order)
		: Message(MSG_LAYER_ORDER, ctx),
		m_order(order)
		{}

	static LayerOrder *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerOrder *fromText(uint8_t ctx, const Kwargs &kwargs);

	const QList<uint16_t> &order() const { return m_order; }

	/**
	 * @brief Get sanitized layer order
	 *
	 * This function checks that the new ordering is valid in respect to the current order
	 * and returns a sanitized ordering.
	 *
	 * The following corrections are made:
	 * - duplicate IDs are removed
	 * - IDs not in the current order are removed
	 * - missing IDs are appended to the new order
	 *
	 * @param currentOrder the current ordering
	 * @return cleaned up ordering
	 */
	QList<uint16_t> sanitizedOrder(const QList<uint16_t> &currentOrder) const;

	QString messageName() const override { return QStringLiteral("layerorder"); }
protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	QList<uint16_t> m_order;
};

/**
 * @brief Layer deletion command
 *
 * If the merge attribute is set, the contents of the layer is merged
 * to the layer below it. Merging the bottom-most layer does nothing.
 *
 * If the current layer or layer controls in general are locked, this command
 * requires session operator privileges.
 */
class LayerDelete final : public Message {
public:
	LayerDelete(uint8_t ctx, uint16_t id, uint8_t merge)
		: Message(MSG_LAYER_DELETE, ctx),
		m_id(id),
		m_merge(merge)
		{}

	static LayerDelete *deserialize(uint8_t ctx, const uchar *data, uint len);
	static LayerDelete *fromText(uint8_t ctx, const Kwargs &kwargs);

	uint16_t layer() const override { return m_id; }
	uint8_t merge() const { return m_merge; }

	QString messageName() const override { return QStringLiteral("deletelayer"); }

protected:
	int payloadLength() const override;
	int serializePayload(uchar *data) const override;
	Kwargs kwargs() const override;

private:
	uint16_t m_id;
	uint8_t m_merge;
};

}

#endif

