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
#ifndef DP_UTIL_IDLIST_H
#define DP_UTIL_IDLIST_H

#include <QList>

/**
 * \brief A class for keeping track of used IDs
 * 
 * This class records ID numbers that are currently in use and that
 * have been used and tries to assign unused ones. If unused IDs have run
 * out, used ID numbers will be given out.
 * TODO ID recycling
 */
class UsedIdList {
public:
	UsedIdList(int max, int min=1) : _min(min), _max(max), _next(min) { Q_ASSERT(min<max); }

	//! Get the next free ID number
	int takeNext();
	
	//! Release the given ID number
	void release(int id);
	
	//! Mark this ID number as being in use
	void reserve(int id);

	//! Reset IDs
	void reset();

private:
	//QList<int> _inuse;
	QList<int> _used;
	const int _min;
	const int _max;
	int _next;
};

#endif

