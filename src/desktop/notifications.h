// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H

namespace notification {

enum class Event {
	CHAT,     // Chat message received
	MARKER,   // Recording playback stopped on a marker
	LOCKED,   // Canvas was locked
	UNLOCKED, // ...unlocked
	LOGIN,    // A user just logged in
	LOGOUT    // ...logged out
};

void playSoundNow(Event event, int volume);

void playSound(Event event);

}

#endif // NOTIFICATIONS_H
