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

#include "desktop/notifications.h"
#include "libshared/util/paths.h"

#include <QSoundEffect>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QDateTime>

namespace notification {

static QMap<Event,QSoundEffect*> sounds;
static qint64 lasttime = 0;

void playSound(Event event)
{
	// Notification rate limiting
	const qint64 t = QDateTime::currentMSecsSinceEpoch();
	if(t - lasttime < 1500)
		return;
	lasttime = t;

	// Check if this notification is enabled
	QSettings cfg;
	cfg.beginGroup("notifications");
	int volume = qBound(0, cfg.value("volume", 40).toInt(), 100);
	if(volume==0)
		return;

	bool enabled = false;
	switch(event) {
	case Event::CHAT: enabled = cfg.value("chat", true).toBool(); break;
	case Event::MARKER: enabled = cfg.value("marker", true).toBool(); break;
	case Event::LOCKED:
	case Event::UNLOCKED:
		enabled = cfg.value("lock", true).toBool();
		break;
	case Event::LOGIN:
	case Event::LOGOUT:
		enabled = cfg.value("login", true).toBool();
		break;
	}
	if(!enabled)
		return;

	// Lazily load the sound effect
	if(!sounds.contains(event)) {
		QString filename;
		switch(event) {
		case Event::CHAT: filename = QStringLiteral("sounds/chat.wav"); break;
		case Event::MARKER: filename = QStringLiteral("sounds/marker.wav"); break;
		case Event::LOCKED: filename = QStringLiteral("sounds/lock.wav"); break;
		case Event::UNLOCKED: filename = QStringLiteral("sounds/unlock.wav"); break;
		case Event::LOGIN: filename = QStringLiteral("sounds/login.wav"); break;
		case Event::LOGOUT: filename = QStringLiteral("sounds/logout.wav"); break;
		}

		Q_ASSERT(!filename.isEmpty());
		if(filename.isEmpty()) {
			qWarning("Sound effect %d not defined!", int(event));
			return;
		}

		const QString fullpath = utils::paths::locateDataFile(filename);
		if(fullpath.isEmpty()) {
			qWarning() << filename << "not found!";
			return;
		}
		
		QSoundEffect *fx = new QSoundEffect;
		fx->setSource(QUrl::fromLocalFile(fullpath));
		sounds[event] = fx;
	}

	// We have a sound effect... play it now
	sounds[event]->setVolume(volume / 100.0);
	sounds[event]->play();
}

}
