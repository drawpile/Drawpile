// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H
#include <QHash>
#include <QObject>
#include <QUrl>
#include <desktop/utils/soundplayer.h>

class MainWindow;
class QWidget;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace notification {

enum class Event : int {
	Chat,		 // Chat message received
	PrivateChat, // ...directed specifically at this user
	Locked,		 // Canvas was locked
	Unlocked,	 // ...unlocked
	Login,		 // A user just logged in
	Logout,		 // ...logged out
	Disconnect,	 // Disconnected from a session
};

class Notifications : public QObject {
	Q_OBJECT
public:
	explicit Notifications(QObject *parent = nullptr);

	void preview(QWidget *widget, Event event, const QString &message);
	void trigger(QWidget *widget, Event event, const QString &message);

private:
	static constexpr qint64 SOUND_DELAY_MSEC = 1500;

	void notify(
		QWidget *widget, Event event, const QString &message, bool isPreview);

	static MainWindow *getMainWindow(QWidget *widget);

	static bool
	isSoundEnabled(const desktop::settings::Settings &settings, Event event);

	static bool
	isPopupEnabled(const desktop::settings::Settings &settings, Event event);

	static bool
	isFlashEnabled(const desktop::settings::Settings &settings, Event event);

	void playSound(Event event, int volume);
	QString getSound(Event event);

	bool isPlayerAvailable();
	static bool isHighPriority(Event event);
	static bool isEmittedDuringCatchup(Event event);

	qint64 m_lastSoundMsec;
	QHash<int, QString> m_sounds;
	SoundPlayer m_soundPlayer;
};

}

#endif
