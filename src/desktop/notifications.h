// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H
#include <QHash>
#include <QObject>

class MainWindow;
class QSoundEffect;
class QWidget;

namespace desktop {
namespace settings {
class Settings;
}
}

namespace notification {

enum class Event : int {
	Chat,		// Chat message received
	Locked,		// Canvas was locked
	Unlocked,	// ...unlocked
	Login,		// A user just logged in
	Logout,		// ...logged out
	Disconnect, // Disconnected from a session
};

class Notifications : public QObject {
	Q_OBJECT
public:
	explicit Notifications(QObject *parent = nullptr);

	void trigger(
		QWidget *widget, Event event, const QString &message,
		bool skipMute = false);

private:
	static constexpr qint64 SOUND_DELAY_MSEC = 1500;

	static MainWindow *getMainWindow(QWidget *widget);

	static bool
	isSoundEnabled(const desktop::settings::Settings &settings, Event event);

	static bool
	isPopupEnabled(const desktop::settings::Settings &settings, Event event);

	static bool
	isFlashEnabled(const desktop::settings::Settings &settings, Event event);

	void playSound(Event event, int volume);
	QSoundEffect *getSound(Event event);

	qint64 m_lastSoundMsec;
	QHash<int, QSoundEffect *> m_sounds;
};

}

#endif
