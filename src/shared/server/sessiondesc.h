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

#ifndef SESSIONDESC_H
#define SESSIONDESC_H

#include <QString>
#include <QMetaType>

namespace server {

class SessionState;

/**
 * @brief Information about an available session
 */
struct SessionDescription {
	int id;
	int protoMinor;
	int userCount;
	QString title;
	QString password;
	bool closed;
	bool persistent;
	bool hibernating;

	SessionDescription();
	explicit SessionDescription(const SessionState &session);
};

}

Q_DECLARE_METATYPE(server::SessionDescription)

#endif
