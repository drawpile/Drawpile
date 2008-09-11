/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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

#ifndef DP_PROTO_UTILS_H
#define DP_PROTO_UTILS_H

#include <QtEndian>

namespace protocol {

/**
 * A few utilities to aid (de)serialization.
 */
struct Utils {
	/**
	 * Write 2 bytes in network byte order.
	 */
	static inline void write16(QIODevice& data, quint16 val) {
		val = qToBigEndian(val);
		data.write((char*)&val, 2);
	}

	/**
	 * Read 2 bytes in network byte order.
	 */
	static inline quint16 read16(QIODevice& data) {
		quint16 val;
		data.read((char*)&val, 2);
		return qFromBigEndian(val);
	}

	/**
	 * Write 4 bytes in network byte order.
	 */
	static inline void write32(QIODevice& data, quint32 val) {
		val = qToBigEndian(val);
		data.write((char*)&val, 4);
	}

	/**
	 * Read 4 bytes in network byte order
	 */
	static inline quint32 read32(QIODevice& data) {
		quint32 val;
		data.read((char*)&val, 4);
		return qFromBigEndian(val);
	}

	/**
	 * Get one byte
	 */
	static inline quint8 read8(QIODevice& data) {
		quint8 val;
		data.getChar((char*)&val);
		return val;
	}
};

}

#endif
