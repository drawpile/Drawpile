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
#ifndef DP_NET_LOGIN_H
#define DP_NET_LOGIN_H

#include <QString>

#include "message.h"

namespace protocol {

class Login : public Message {
public:
	Login(const QByteArray &msg) : Message(MSG_LOGIN), _msg(msg) {}
	Login(const QString &msg) : Login(msg.toUtf8()) {}

	static Login *deserialize(const uchar *data, uint len);

	QString message() const { return QString::fromUtf8(_msg); }

protected:
    int payloadLength() const;
	int serializePayload(uchar *data) const;

private:
    QByteArray _msg;
};

}

#endif
