/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef CHANGEFLAGS_H
#define CHANGEFLAGS_H

/**
 * A set of changes to a bitfield
 */
template<typename Flags>
class ChangeFlags {
public:
	ChangeFlags() : m_mask(0), m_changes(0) { }

	ChangeFlags &set(Flags flag, bool value)
	{
		m_mask |= flag;
		if(value)
			m_changes |= flag;
		else
			m_changes &= ~flag;

		return *this;
	}

	/**
	 * Return the given bitfield with the changes applied
	 */
	Flags update(Flags oldFlags) const
	{
		return (oldFlags & ~m_mask) | m_changes;
	}

private:
	Flags m_mask;
	Flags m_changes;
};

#endif

