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

#include "reader-compat.h"

#include "../net/message.h"
#include "../net/pen.h"

namespace recording {
namespace compat {

using namespace protocol;

Message *deserializeV11(const uchar *data, int length)
{
	// The only relevant difference between V11.x and V14.4 is that
	// brush size is now treated as the diameter rather than the radius,
	// so we need to double the size.

	Message *m = Message::deserialize(data, length);

	if(m && m->type() == MSG_TOOLCHANGE) {
		const ToolChange &t = static_cast<ToolChange&>(*m);

		Message *m2 = new ToolChange(
			t.contextId(),
			t.layer(),
			t.blend(),
			t.mode(),
			t.spacing(),
			t.color_h(),
			t.color_l(),
			t.hard_h(),
			t.hard_l(),
			qMax(1, t.size_h()*2), // radius 0 meant diameter 1 brush
			qMax(1, t.size_l()*2),
			t.opacity_h(),
			t.opacity_l());
		delete m;
		m = m2;
	}

	return m;
}

}
}

