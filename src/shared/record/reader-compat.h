/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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
#ifndef REC_READER_COMPAT_H
#define REC_READER_COMPAT_H

#include <Qt>

namespace protocol {
	class Message;
}

namespace recording {
namespace compat {

// Protocol version 14.x and 13.x messages
protocol::Message *deserializeV14(const uchar *data, int length);

// Protocol version 12.x messages
protocol::Message *deserializeV12(const uchar *data, int length);

// Protocol version 11.x messages
protocol::Message *deserializeV11(const uchar *data, int length);

// Protocol version 10.2 -- 7.1 messages
protocol::Message *deserializeV10(const uchar *data, int length);

}

}

#endif

