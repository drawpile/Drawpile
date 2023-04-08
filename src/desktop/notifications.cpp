// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/notifications.h"
#include "libshared/util/paths.h"
#include "desktop/main.h"

#include <QSoundEffect>
#include <QMap>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QSettings>
#include <QDateTime>

namespace notification {

static qint64 lasttime = 0;

void playSoundNow(Event event, int volume)
{
	if(volume <= 0) {
		return;
	}

	// Lazily load the sound effect
	DrawpileApp *app = static_cast<DrawpileApp *>(qApp);
	if(!app->m_sounds.contains(event)) {
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
		app->m_sounds[event] = fx;
	}

	// We have a sound effect... play it now
	app->m_sounds[event]->setVolume(qMin(volume, 100) / 100.0);
	app->m_sounds[event]->play();
}

static bool isEnabled(Event event, QSettings &cfg)
{
	switch(event) {
	case Event::CHAT:
		return cfg.value("chat", true).toBool(); break;
	case Event::MARKER:
		return cfg.value("marker", true).toBool(); break;
	case Event::LOCKED:
		return cfg.value("lock", true).toBool();
	case Event::UNLOCKED:
		// Used to be the same value as lock, so default to that if not found.
		return cfg.value("unlock", cfg.value("lock", true).toBool()).toBool();
	case Event::LOGIN:
		return cfg.value("login", true).toBool();
	case Event::LOGOUT:
		// Used to be the same value as login, so default to that if not found.
		return cfg.value("logout", cfg.value("login", true).toBool()).toBool();
	default:
		return false;
	}
}

void playSound(Event event)
{
	// Notification rate limiting
	const qint64 t = QDateTime::currentMSecsSinceEpoch();
	if(t - lasttime >= 1500) {
		QSettings cfg;
		if(isEnabled(event, cfg)) {
			playSoundNow(event, cfg.value("volume", 40).toInt());
		}
	}
}

}
