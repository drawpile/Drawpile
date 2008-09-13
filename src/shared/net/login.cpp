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

#include <QIODevice>

#include "constants.h"
#include "login.h"
#include "utils.h"

namespace protocol {

LoginId::LoginId(const char magic[4], int rev, int ver) : Packet(LOGIN_ID),      _rev(rev), _ver(ver)
{
	_magic[0] = magic[0];
	_magic[1] = magic[1];
	_magic[2] = magic[2];
	_magic[3] = magic[3];
}

LoginId::LoginId(int ver) : Packet(LOGIN_ID), _rev(REVISION), _ver(ver) {
	_magic[0] = MAGIC[0];
	_magic[1] = MAGIC[1];
	_magic[2] = MAGIC[2];
	_magic[3] = MAGIC[3];
}

LoginId *LoginId::deserialize(QIODevice& data, int len) {
	char magic[4];
	data.read(magic, 4);
	int rev = utils::read16(data);
	int ver = utils::read16(data);
	return new LoginId(magic, rev, ver);
}

void LoginId::serializeBody(QIODevice& data) const {
	data.write(_magic, 4);
	utils::write16(data, _rev);
	utils::write16(data, _ver);
}

bool LoginId::isCompatible() const {
	return _magic[0] == MAGIC[0] &&
			_magic[1] == MAGIC[1] &&
			_magic[2] == MAGIC[2] &&
			_magic[3] == MAGIC[3] &&
			REVISION == _rev;
}

}

