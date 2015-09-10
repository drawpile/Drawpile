/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef DP_NET_LAYER_H
#define DP_NET_LAYER_H

#include <cstdint>
#include <QString>
#include <QList>

#include "message.h"

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
class CanvasResize : public Message {
public:
	CanvasResize(uint8_t ctx, int32_t top, int32_t right, int32_t bottom, int32_t left)
		: Message(MSG_CANVAS_RESIZE, ctx), _top(top), _right(right), _bottom(bottom), _left(left)
		{}

	static CanvasResize *deserialize(uint8_t ctx, const uchar *data, uint len);

	int32_t top() const { return _top; }
	int32_t right() const { return _right; }
	int32_t bottom() const { return _bottom; }
	int32_t left() const { return _left; }

	bool isOpCommand() const { return true; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;	

private:
	int32_t _top;
	int32_t _right;
	int32_t _bottom;
	int32_t _left;
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
class LayerCreate : public Message {
public:
	static const uint8_t FLAG_COPY = 0x01;
	static const uint8_t FLAG_INSERT = 0x02;

	LayerCreate(uint8_t ctxid, uint16_t id, uint16_t source, uint32_t fill, uint8_t flags, const QString &title)
		: Message(MSG_LAYER_CREATE, ctxid), _id(id), _source(source), _fill(fill), _flags(flags), _title(title.toUtf8())
		{}

	static LayerCreate *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	uint16_t source() const { return _source; }
	uint32_t fill() const { return _fill; }
	uint8_t flags() const { return _flags; }
	QString title() const { return QString::fromUtf8(_title); }

	/**
	 * @brief Check if the ID's namespace portition matches the context ID
	 *
	 * Note. This check is only needed during normal multiuser operation. Layers
	 * created in single-user mode can use any ID.
	 * This means layer IDs of the initial snapshot need not be validated.
	 */
	bool isValidId() const { return (id()>>8) == contextId(); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	uint16_t _source;
	uint32_t _fill;
	uint8_t _flags;
	QByteArray _title;
};

/**
 * @brief Layer attribute change command
 *
 * If the current layer or layer controls in general are locked, this command
 * requires session operator privileges.
 */
class LayerAttributes : public Message {
public:
	LayerAttributes(uint8_t ctx, uint16_t id, uint8_t opacity, uint8_t blend)
		: Message(MSG_LAYER_ATTR, ctx), _id(id),
		_opacity(opacity), _blend(blend)
		{}

	static LayerAttributes *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	uint8_t opacity() const { return _opacity; }
	uint8_t blend() const { return _blend; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	uint8_t _opacity;
	uint8_t _blend;
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
class LayerVisibility : public Message {
public:
	LayerVisibility(uint8_t ctx, uint16_t id, uint8_t visible)
		: Message(MSG_LAYER_VISIBILITY, ctx), m_id(id), m_visible(visible)
	{ }

	static LayerVisibility *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return m_id; }
	uint8_t visible() const { return m_visible; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

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
class LayerRetitle : public Message {
public:
	LayerRetitle(uint8_t ctx, uint16_t id, const QByteArray &title)
		: Message(MSG_LAYER_RETITLE, ctx), _id(id), _title(title)
		{}
	LayerRetitle(uint8_t ctx, uint16_t id, const QString &title)
		: LayerRetitle(ctx, id, title.toUtf8())
		{}

	static LayerRetitle *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	QString title() const { return QString::fromUtf8(_title); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	QByteArray _title;
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
class LayerOrder : public Message {
public:
	LayerOrder(uint8_t ctx, const QList<uint16_t> &order)
		: Message(MSG_LAYER_ORDER, ctx),
		_order(order)
		{}
	
	static LayerOrder *deserialize(uint8_t ctx, const uchar *data, uint len);

	const QList<uint16_t> &order() const { return _order; }

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

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QList<uint16_t> _order;
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
class LayerDelete : public Message {
public:
	LayerDelete(uint8_t ctx, uint16_t id, uint8_t merge)
		: Message(MSG_LAYER_DELETE, ctx),
		_id(id),
		_merge(merge)
		{}

	static LayerDelete *deserialize(uint8_t ctx, const uchar *data, uint len);

	uint16_t id() const { return _id; }
	uint8_t merge() const { return _merge; }

	bool isUndoable() const { return true; }
	
protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _id;
	uint8_t _merge;
};

}

#endif
