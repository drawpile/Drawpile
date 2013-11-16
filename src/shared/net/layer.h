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
#include <QVector>

#include "message.h"

namespace protocol {

/**
 * \brief Canvas resize command
 * 
 * This affects the size of all existing and future layers.
 */
class CanvasResize : public Message {
public:
	CanvasResize(uint16_t width, uint16_t height)
		: Message(MSG_CANVAS_RESIZE), _width(width), _height(height)
		{}

	uint16_t width() const { return _width; }
	uint16_t height() const { return _height; }
		
protected:
	int payloadLength() const;

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
	LayerCreate(uint8_t id, uint32_t fill, const QString &title)
		: Message(MSG_LAYER_CREATE), _id(id), _fill(fill), _title(title.toUtf8())
		{}

	uint8_t id() const { return _id; }
	uint32_t fill() const { return _fill; }
	const QByteArray &title() const { return _title; }

protected:
	int payloadLength() const;

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
	LayerAttributes(uint8_t id, uint8_t opacity, uint8_t blend, const QString &title)
		: Message(MSG_LAYER_CREATE), _id(id),
		_opacity(opacity), _blend(blend), _title(title.toUtf8())
		{}

	uint8_t id() const { return _id; }
	uint8_t opacity() const { return _opacity; }
	uint8_t blend() const { return _blend; }
	QString title() const { return QString::fromUtf8(_title); }

protected:
	int payloadLength() const;

private:
	uint8_t _id;
	uint8_t _opacity;
	uint8_t _blend;
	QByteArray _title;
};

/**
 * \brief Layer order change command
 */
class LayerOrder : public Message {
public:
	LayerOrder(const QList<uint8_t> &order)
		: Message(MSG_LAYER_ORDER),
		_order(order)
		{}
	
	const QList<uint8_t> &order() const { return _order; }
	
protected:
	int payloadLength() const;

private:
	QList<uint8_t> _order;
};

/**
 * \brief Layer deletion command
 */
class LayerDelete : public Message {
public:
	LayerDelete(uint8_t id)
		: Message(MSG_LAYER_DELETE),
		_id(id)
		{}
	
	uint8_t id() const { return _id; }
	
protected:
	int payloadLength() const;

private:
	uint8_t _id;
};

}

#endif
