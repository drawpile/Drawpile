/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008-2014 Calle Laakkonen

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
#ifndef IOUTILS_H
#define IOUTILS_H

#include <QtEndian>
#include <QIODevice>

namespace ioutils {

inline uchar read8(QIODevice *in) {
	char c;
	in->getChar(&c);
	return c;
}

inline quint16 read16(QIODevice *in) {
	char c[2];
	in->read(c, 2);
	return qFromBigEndian<quint16>((uchar*)c);
}

inline quint32 read32(QIODevice *in) {
	char c[4];
	in->read(c, 4);
	return qFromBigEndian<quint32>((uchar*)c);
}

inline qint64 read64(QIODevice *in) {
	char c[8];
	in->read(c, 8);
	return qFromBigEndian<qint64>((uchar*)c);
}

inline void write16(QIODevice *out, quint16 i)
{
	uchar buf[2];
	qToBigEndian(i, buf);
	out->write((const char*)buf, 2);
}

inline void write32(QIODevice *out, quint32 i)
{
	uchar buf[4];
	qToBigEndian(i, buf);
	out->write((const char*)buf, 4);
}

inline void write64(QIODevice *out, qint64 i)
{
	uchar buf[8];
	qToBigEndian(i, buf);
	out->write((const char*)buf, 8);
}

}

#endif
