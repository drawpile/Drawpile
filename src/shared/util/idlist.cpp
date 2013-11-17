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

#include "idlist.h"

int UsedIdList::takeNext()
{
	// TODO start giving out release IDs after unused ones run out
	const int oldnext = _next;
	do {
		if(!_used.contains(_next)) {
			reserve(_next);
			return _next;
		}

		++_next;
		if(_next>_max)
			_next = _min;
	} while(_next != oldnext);

	// TODO handle this properly
	return _min-1;
}
	
void UsedIdList::release(int id)
{
	// TODO
#if 0
	Q_ASSERT(_inuse.contains(id));
	_inuse.remove(id);
	if(!_used.contains(id))
		_used.append(id);
#endif
}
	
void UsedIdList::reserve(int id)
{
#if 0
	Q_ASSERT(!_inuse.contains(id));
	if(!_inuse.contains(id))
		_inuse.append(id);
	_used.remove(id);
#else
	Q_ASSERT(!_used.contains(id));
	_used.append(id);
#endif
}

void UsedIdList::reset()
{
	//_inuse.clear();
	_used.clear();
	_next = _min;
}

