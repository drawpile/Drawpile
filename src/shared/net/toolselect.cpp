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

#include "toolselect.h"
#include "utils.h"

namespace protocol {

ToolSelect *ToolSelect::deserialize(QIODevice& data, int len) {
	Q_ASSERT(len == 16);
	int user = Utils::read8(data);
	int tool = Utils::read8(data);
	int mode = Utils::read8(data);
	quint32 c1 = Utils::read32(data);
	quint32 c0 = Utils::read32(data);
	int s1 = Utils::read8(data);
	int s0 = Utils::read8(data);
	int h1 = Utils::read8(data);
	int h0 = Utils::read8(data);
	int space = Utils::read8(data);
	return new ToolSelect(user, tool, mode, c1, c0, s1, s0, h1, h0, space);
}

void ToolSelect::serializeBody(QIODevice& data) const {
	data.putChar(_user);
	data.putChar(_tool);
	data.putChar(_mode);
	Utils::write32(data, _c1);
	Utils::write32(data, _c0);
	data.putChar(_s1);
	data.putChar(_s0);
	data.putChar(_h1);
	data.putChar(_h0);
	data.putChar(_space);
}

}

