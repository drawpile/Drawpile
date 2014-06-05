/*
   DrawPile - a collaborative drawing program.

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

#ifndef RECORD_HIBERNATE_H
#define RECORD_HIBERNATE_H

#include <QString>

namespace recording {

/**
 * @brief Session hibernation file header
 *
 * A hibernation file differs from a regular session recording in two ways:
 * first, it has (this) extra header and second, all meta messages are recorded.
 */
struct HibernationHeader {
	enum Flag {
		NOFLAGS = 0,
		PERSISTENT = 0x01,
		PASSWORD = 0x02
	};
	Q_DECLARE_FLAGS(Flags, Flag)

	//! Protocol minor version for the session
	int minorVersion;

	//! Session title
	QString title;

	//! Session flags
	Flags flags;

	HibernationHeader() : minorVersion(0), flags(NOFLAGS) { }
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HibernationHeader::Flags)

}

#endif
