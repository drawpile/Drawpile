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

typedef uint8_t LayerId;

/**
 * \brief Layer creation command.
 * 
 */
class LayerCreate : public Message {
public:
	LayerCreate(LayerId id, uint32_t fill)
		: Message(MSG_LAYER_CREATE), _id(id), _fill(fill)
		{}

	LayerId id() const { return _id; }
	uint32_t fill() const { return _fill; }

protected:
	int payloadLength() const;

private:
	const LayerId _id;
	const uint32_t _fill;
};

/**
 * \brief Layer attribute change command
 */
class LayerAttributes : public Message {
public:
	LayerAttributes(LayerId id, uint8_t opacity, uint8_t blend, const QString &title)
		: Message(MSG_LAYER_CREATE), _id(id),
		_opacity(opacity), _blend(blend), _title(title.toUtf8())
		{}

	LayerId id() const { return _id; }
	uint8_t opacity() const { return _opacity; }
	uint8_t blend() const { return _blend; }
	QString title() const { return QString::fromUtf8(_title); }

protected:
	int payloadLength() const;

private:
	const LayerId _id;
	const uint8_t _opacity;
	const uint8_t _blend;
	const QByteArray _title;
};

typedef QVector<LayerId> LayerOrderVector;

/**
 * \brief Layer order change command
 */
class LayerOrder : public Message {
public:
	LayerOrder(const LayerOrderVector &order)
		: Message(MSG_LAYER_ORDER),
		_order(order)
		{}
	
	const LayerOrderVector &order() const { return _order; }
	
protected:
	int payloadLength() const;

private:
	LayerOrderVector _order;
};

/**
 * \brief Layer deletion command
 */
class LayerDelete : public Message {
public:
	LayerDelete(LayerId id)
		: Message(MSG_LAYER_DELETE),
		_id(id)
		{}
	
	LayerId id() const { return _id; }
	
protected:
	int payloadLength() const;

private:
	const LayerId _id;
};

}

#endif
