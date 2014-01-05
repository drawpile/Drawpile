/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
	const int total = _max - _min + 1;
	// Find an unused ID if available
	if(_used.size() < total) {
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
	}

	// Ok, all IDs have been used at least once. Find a free one
	if(_inuse.size() < total) {
		const int oldnext = _next;
		do {
			if(!_inuse.contains(_next)) {
				reserve(_next);
				return _next;
			}

			++_next;
			if(_next>_max)
				_next = _min;
		} while(_next != oldnext);
	}

	// None available
	return _min-1;
}
	
void UsedIdList::release(int id)
{
	Q_ASSERT(_inuse.contains(id));
	_inuse.removeOne(id);
}
	
void UsedIdList::reserve(int id)
{
	if(!_inuse.contains(id))
		_inuse.append(id);
	if(!_used.contains(id))
		_used.append(id);
}

void UsedIdList::markUsed(int id)
{
	if(!_used.contains(id))
		_used.append(id);
}

void UsedIdList::reset()
{
	_inuse.clear();
	_used.clear();
	_next = _min;
}
