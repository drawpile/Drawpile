// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/notifications.h"
#include "desktop/main.h"
#include "desktop/mainwindow.h"
#include "libclient/config/config.h"
#include "libshared/util/paths.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMap>

namespace notification {

Notifications::Notifications(QObject *parent)
	: QObject(parent)
	, m_lastSoundMsec(QDateTime::currentMSecsSinceEpoch())
{
}

void Notifications::preview(
	QWidget *widget, Event event, const QString &message)
{
	notify(widget, event, message, true);
}

void Notifications::trigger(
	QWidget *widget, Event event, const QString &message)
{
	notify(widget, event, message, false);
}

QString Notifications::soundPlayerBackend() const
{
	return m_soundPlayer.getBackendName();
}

void Notifications::notify(
	QWidget *widget, Event event, const QString &message, bool isPreview)
{
	MainWindow *mw = getMainWindow(widget);
	if(!mw) {
		qWarning("No main window found for event %d", int(event));
	} else if(
		!isPreview &&
		(mw->notificationsMuted() ||
		 (mw->isInitialCatchup() && !isEmittedDuringCatchup(event)))) {
		return;
	}

	qint64 now = QDateTime::currentMSecsSinceEpoch();
	config::Config *cfg = dpAppConfig();
	bool shouldPlay =
		isSoundEnabled(cfg, event) &&
		(isPreview || isHighPriority(event) ||
		 (now - m_lastSoundMsec >= SOUND_DELAY_MSEC && isPlayerAvailable()));
	if(shouldPlay) {
		int volume = cfg->getSoundVolume();
		if(volume > 0) {
			m_lastSoundMsec = now;
			playSound(event, qBound(0, volume, 100));
		}
	}

	if(mw && isPopupEnabled(cfg, event)) {
		mw->showPopupMessage(message);
	}

	if(mw && isFlashEnabled(cfg, event)) {
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

bool Notifications::isSoundEnabled(config::Config *cfg, Event event)
{
	switch(event) {
	case Event::Chat:
		return cfg->getNotifSoundChat();
	case Event::PrivateChat:
		return cfg->getNotifSoundPrivateChat();
	case Event::Locked:
		return cfg->getNotifSoundLock();
	case Event::Unlocked:
		return cfg->getNotifSoundUnlock();
	case Event::Login:
		return cfg->getNotifSoundLogin();
	case Event::Logout:
		return cfg->getNotifSoundLogout();
	case Event::Disconnect:
		return cfg->getNotifSoundDisconnect();
	}
	qWarning("Unknown sound event %d", int(event));
	return false;
}

bool Notifications::isPopupEnabled(config::Config *cfg, Event event)
{
	switch(event) {
	case Event::Chat:
		return cfg->getNotifPopupChat();
	case Event::PrivateChat:
		return cfg->getNotifPopupPrivateChat();
	case Event::Locked:
		return cfg->getNotifPopupLock();
	case Event::Unlocked:
		return cfg->getNotifPopupUnlock();
	case Event::Login:
		return cfg->getNotifPopupLogin();
	case Event::Logout:
		return cfg->getNotifPopupLogout();
	case Event::Disconnect:
		return cfg->getNotifPopupDisconnect();
	}
	qWarning("Unknown popup event %d", int(event));
	return false;
}

bool Notifications::isFlashEnabled(config::Config *cfg, Event event)
{
	switch(event) {
	case Event::Chat:
		return cfg->getNotifFlashChat();
	case Event::PrivateChat:
		return cfg->getNotifFlashPrivateChat();
	case Event::Locked:
		return cfg->getNotifFlashLock();
	case Event::Unlocked:
		return cfg->getNotifFlashUnlock();
	case Event::Login:
		return cfg->getNotifFlashLogin();
	case Event::Logout:
		return cfg->getNotifFlashLogout();
	case Event::Disconnect:
		return cfg->getNotifFlashDisconnect();
	}
	qWarning("Unknown flash event %d", int(event));
	return false;
}

void Notifications::playSound(Event event, int volume)
{
	m_soundPlayer.playSound(getSound(event), volume);
}

QString Notifications::getSound(Event event)
{
	int key = int(event);
	if(m_sounds.contains(key)) {
		return m_sounds[key];
	} else {
		QString filename;
		switch(event) {
		case Event::Chat:
			filename = QStringLiteral("sounds/notif-chat.wav");
			break;
		case Event::PrivateChat:
			filename = QStringLiteral("sounds/notif-private-chat.wav");
			break;
		case Event::Locked:
			filename = QStringLiteral("sounds/notif-lock.wav");
			break;
		case Event::Unlocked:
			filename = QStringLiteral("sounds/notif-unlock.wav");
			break;
		case Event::Login:
			filename = QStringLiteral("sounds/notif-login.wav");
			break;
		case Event::Logout:
			filename = QStringLiteral("sounds/notif-logout.wav");
			break;
		case Event::Disconnect:
			filename = QStringLiteral("sounds/notif-disconnect.wav");
			break;
		}

		if(filename.isEmpty()) {
			qWarning("Sound effect %d not defined", int(event));
			m_sounds[key] = QString();
			return QString();
		}

		QString path = utils::paths::locateDataFile(filename);
		if(path.isEmpty()) {
			qWarning("Sound file '%s' not found", qUtf8Printable(filename));
			m_sounds[key] = QString();
			return QString();
		}

		m_sounds[key] = path;
		return path;
	}
}

bool Notifications::isPlayerAvailable()
{
	return !m_soundPlayer.isPlaying();
}

bool Notifications::isHighPriority(Event event)
{
	// There's usually a chat sound played right before the disconnect event,
	// which would prevent the more important disconnect sound from playing.
	// So we treat that one as high-priority, making it stop the other sound.
	return event == Event::Disconnect;
}

bool Notifications::isEmittedDuringCatchup(Event event)
{
	switch(event) {
	case Event::Chat:
	case Event::PrivateChat:
	case Event::Locked:
	case Event::Unlocked:
	case Event::Login:
	case Event::Logout:
		return false;
	case Event::Disconnect:
		return true;
	}
	qWarning("Unknown emitted during catchup event %d", int(event));
	return false;
}

}
