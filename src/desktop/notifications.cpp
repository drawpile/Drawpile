// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/notifications.h"
#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "libshared/util/paths.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QSoundEffect>

namespace notification {

Notifications::Notifications(QObject *parent)
	: QObject(parent)
	, m_lastSoundMsec(QDateTime::currentMSecsSinceEpoch())
{
}

void Notifications::trigger(
	QWidget *widget, Event event, const QString &message, bool skipMute)
{
	MainWindow *mw = getMainWindow(widget);
	if(!mw) {
		qWarning("No main window found for event %d", int(event));
	} else if(!skipMute && mw->notificationsMuted()) {
		return;
	}

	qint64 now = QDateTime::currentMSecsSinceEpoch();
	desktop::settings::Settings &settings = dpApp().settings();
	int volume;
	if(now - m_lastSoundMsec >= SOUND_DELAY_MSEC) {
		volume = settings.soundVolume();
		m_lastSoundMsec = now;
	} else {
		volume = 0;
	}

	if(volume > 0 && isSoundEnabled(settings, event)) {
		playSound(event, volume);
	}

	if(mw && isPopupEnabled(settings, event)) {
		mw->showPopupMessage(message);
	}

	if(mw && isFlashEnabled(settings, event)) {
		QApplication::alert(mw);
	}
}

MainWindow *Notifications::getMainWindow(QWidget *widget)
{
	MainWindow *mw = nullptr;
	while(widget && !(mw = qobject_cast<MainWindow *>(widget))) {
		widget = widget->parentWidget();
	}
	return mw;
}

bool Notifications::isSoundEnabled(
	const desktop::settings::Settings &settings, Event event)
{
	switch(event) {
	case Event::Chat:
		return settings.notifSoundChat();
	case Event::Locked:
		return settings.notifSoundLock();
	case Event::Unlocked:
		return settings.notifSoundUnlock();
	case Event::Login:
		return settings.notifSoundLogin();
	case Event::Logout:
		return settings.notifSoundLogout();
	case Event::Disconnect:
		return settings.notifSoundDisconnect();
	}
	qWarning("Unknown sound event %d", int(event));
	return false;
}

bool Notifications::isPopupEnabled(
	const desktop::settings::Settings &settings, Event event)
{
	switch(event) {
	case Event::Chat:
		return settings.notifPopupChat();
	case Event::Locked:
		return settings.notifPopupLock();
	case Event::Unlocked:
		return settings.notifPopupUnlock();
	case Event::Login:
		return settings.notifPopupLogin();
	case Event::Logout:
		return settings.notifPopupLogout();
	case Event::Disconnect:
		return settings.notifPopupDisconnect();
	}
	qWarning("Unknown popup event %d", int(event));
	return false;
}

bool Notifications::isFlashEnabled(
	const desktop::settings::Settings &settings, Event event)
{
	switch(event) {
	case Event::Chat:
		return settings.notifFlashChat();
	case Event::Locked:
		return settings.notifFlashLock();
	case Event::Unlocked:
		return settings.notifFlashUnlock();
	case Event::Login:
		return settings.notifFlashLogin();
	case Event::Logout:
		return settings.notifFlashLogout();
	case Event::Disconnect:
		return settings.notifFlashDisconnect();
	}
	qWarning("Unknown flash event %d", int(event));
	return false;
}

void Notifications::playSound(Event event, int volume)
{
	QSoundEffect *sound = getSound(event);
	if(sound && !sound->isPlaying()) {
		sound->setVolume(float(qBound(0, volume, 100)) / 100.0f);
		sound->play();
	}
}

QSoundEffect *Notifications::getSound(Event event)
{
	int key = int(event);
	if(m_sounds.contains(key)) {
		return m_sounds[key];
	} else {
		QString filename;
		switch(event) {
		case Event::Chat:
			filename = QStringLiteral("sounds/chat.wav");
			break;
		case Event::Locked:
			filename = QStringLiteral("sounds/lock.wav");
			break;
		case Event::Unlocked:
			filename = QStringLiteral("sounds/unlock.wav");
			break;
		case Event::Login:
			filename = QStringLiteral("sounds/login.wav");
			break;
		case Event::Logout:
			filename = QStringLiteral("sounds/logout.wav");
			break;
		case Event::Disconnect:
			filename = QStringLiteral("sounds/marker.wav");
			break;
		}

		if(filename.isEmpty()) {
			qWarning("Sound effect %d not defined", int(event));
			m_sounds[key] = nullptr;
			return nullptr;
		}

		QString path = utils::paths::locateDataFile(filename);
		if(path.isEmpty()) {
			qWarning("Sound file '%s' not found", qUtf8Printable(filename));
			m_sounds[key] = nullptr;
			return nullptr;
		}

		QSoundEffect *sound = new QSoundEffect(this);
		sound->setSource(QUrl::fromLocalFile(path));
		m_sounds[key] = sound;
		return sound;
	}
}

}
