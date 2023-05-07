// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/notifications.h"
#include "libshared/util/paths.h"

#include <QSoundEffect>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QDateTime>

namespace notification {

static qint64 lasttime = 0;

void playSoundNow(Event event, int volume)
{
	if(volume <= 0) {
		return;
	}

	// Lazily load the sound effect
	auto &app = dpApp();
	if(!app.m_sounds.contains(event)) {
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

		QSoundEffect *fx = new QSoundEffect(qApp);
		fx->setSource(QUrl::fromLocalFile(fullpath));
		app.m_sounds[event] = fx;
	}

	// We have a sound effect... play it now
	app.m_sounds[event]->setVolume(qMin(volume, 100) / 100.0);
	app.m_sounds[event]->play();
}

static bool isEnabled(Event event)
{
	auto &settings = dpApp().settings();
	switch(event) {
	case Event::CHAT:
		return settings.notificationChat();
	case Event::MARKER:
		return settings.notificationMarker();
	case Event::LOCKED:
		return settings.notificationLock();
	case Event::UNLOCKED:
		return settings.notificationUnlock();
	case Event::LOGIN:
		return settings.notificationLogin();
	case Event::LOGOUT:
		return settings.notificationLogout();
	}
	Q_UNREACHABLE();
}

void playSound(Event event)
{
	// Notification rate limiting
	const qint64 t = QDateTime::currentMSecsSinceEpoch();
	if(t - lasttime >= 1500) {
		if(isEnabled(event)) {
			playSoundNow(event, dpApp().settings().soundVolume());
		}
	}
}

}
