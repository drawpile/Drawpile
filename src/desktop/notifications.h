/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

namespace notification {

enum class Event {
	CHAT,     // Chat message received
	MARKER,   // Recording playback stopped on a marker
	LOCKED,   // Active layer was locked
	UNLOCKED, // ...unlocked
	LOGIN,    // A user just logged in
	LOGOUT    // ...logged out
};

void playSoundNow(Event event, int volume);

void playSound(Event event);

void setVolume(int volume);

}

#endif // NOTIFICATIONS_H
