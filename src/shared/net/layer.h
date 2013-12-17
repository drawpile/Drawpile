/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef DP_NET_LAYER_H
#define DP_NET_LAYER_H

#include <cstdint>
#include <QString>
#include <QList>

#include "message.h"

namespace protocol {

/**
 * \brief Canvas resize command
 * 
 * This affects the size of all existing and future layers.
 */
class CanvasResize : public Message {
public:
	CanvasResize(uint8_t ctx, uint16_t width, uint16_t height)
		: Message(MSG_CANVAS_RESIZE, ctx), _width(width), _height(height)
		{}

	static CanvasResize *deserialize(const uchar *data, uint len);

	uint16_t width() const { return _width; }
	uint16_t height() const { return _height; }
		
protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint16_t _width;
	uint16_t _height;
};

/**
 * \brief Layer creation command.
 * 
 */
class LayerCreate : public Message {
public:
	LayerCreate(uint8_t ctxid, uint8_t id, uint32_t fill, const QString &title)
		: Message(MSG_LAYER_CREATE, ctxid), _id(id), _fill(fill), _title(title.toUtf8())
		{}

	static LayerCreate *deserialize(const uchar *data, uint len);

	/**
	 * \brief ID of the newly created layer.
	 * 
	 * When this command is sent by the client, the ID should be set to zero
	 * to indicate the server should assign a free ID. An exception to this
	 * is when generating a snapshot: in that case the client is free to use any
	 * available ID number. (Typically when creating the snapshot, the layers have
	 * already been assigned IDs by the server, or the session has just started meaning
	 * all IDs are free.)
	 * 
	 * @return layer ID number
	 */
	uint8_t id() const { return _id; }

	void setId(uint8_t id) { _id = id; }

	/**
	 * \brief Initial fill color
	 * 
	 * The layer is filled with this color when created.
	 * @return fill color (ARGB)
	 */
	uint32_t fill() const { return _fill; }
	
	/**
	 * \brief Layer title (UTF-8 encoded)
	 * @return layer title
	 */
	QString title() const { return QString::fromUtf8(_title); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint32_t _fill;
	QByteArray _title;
};

/**
 * \brief Layer attribute change command
 */
class LayerAttributes : public Message {
public:
	LayerAttributes(uint8_t ctx, uint8_t id, uint8_t opacity, uint8_t blend)
		: Message(MSG_LAYER_ATTR, ctx), _id(id),
		_opacity(opacity), _blend(blend)
		{}

	static LayerAttributes *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	uint8_t opacity() const { return _opacity; }
	uint8_t blend() const { return _blend; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint8_t _opacity;
	uint8_t _blend;
};

/**
 * \brief Layer title change command
 */
class LayerRetitle : public Message {
public:
	LayerRetitle(uint8_t ctx, uint8_t id, const QByteArray &title)
		: Message(MSG_LAYER_RETITLE, ctx), _id(id), _title(title)
		{}
	LayerRetitle(uint8_t ctx, uint8_t id, const QString &title)
		: LayerRetitle(ctx, id, title.toUtf8())
		{}

	static LayerRetitle *deserialize(const uchar *data, uint len);

	uint8_t id() const { return _id; }
	QString title() const { return QString::fromUtf8(_title); }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	QByteArray _title;
};

/**
 * \brief Layer order change command
 */
class LayerOrder : public Message {
public:
	LayerOrder(uint8_t ctx, const QList<uint8_t> &order)
		: Message(MSG_LAYER_ORDER, ctx),
		_order(order)
		{}
	
	static LayerOrder *deserialize(const uchar *data, uint len);

	const QList<uint8_t> &order() const { return _order; }
	void setOrder(const QList<uint8_t> order) { _order = order; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	QList<uint8_t> _order;
};

/**
 * \brief Layer deletion command
 */
class LayerDelete : public Message {
public:
	LayerDelete(uint8_t ctx, uint8_t id, uint8_t merge)
		: Message(MSG_LAYER_DELETE, ctx),
		_id(id),
		_merge(merge)
		{}

	static LayerDelete *deserialize(const uchar *data, uint len);
	
	bool isOpCommand() const { return true; }

	uint8_t id() const { return _id; }
	uint8_t merge() const { return _merge; }
	
protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;
	bool isUndoable() const { return true; }

private:
	uint8_t _id;
	uint8_t _merge;
};

/**
 * @brief Change layer access control list
 * This is a meta command.
 */
class LayerACL : public Message {
public:
	LayerACL(uint8_t ctx, uint8_t id, uint8_t locked, const QList<uint8_t> &exclusive)
		: Message(MSG_LAYER_ACL, ctx), _id(id), _locked(locked), _exclusive(exclusive)
	{}

	static LayerACL *deserialize(const uchar *data, uint len);

	bool isOpCommand() const { return true; }

	uint8_t id() const { return _id; }
	uint8_t locked() const { return _locked; }
	const QList<uint8_t> exclusive() const { return _exclusive; }

protected:
	int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
	uint8_t _id;
	uint8_t _locked;
	QList<uint8_t> _exclusive;
};

}

#endif
