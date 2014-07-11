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
#ifndef RECORDING_UTILS_H
#define RECORDING_UTILS_H

namespace recording {

inline quint32 version32(quint16 major, quint16 minor) {
	return major << 16 | minor;
}

inline quint16 majorVersion(quint32 version) {
	return version >> 16;
}

inline quint16 minorVersion(quint32 version) {
	return version & 0xffff;
}

}

#endif
