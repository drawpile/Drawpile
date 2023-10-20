// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef NOTIFICATIONS_H
#define NOTIFICATIONS_H
#include <QHash>
#include <QObject>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#	include <QUrl>
#else
#	include <QMediaContent>
#endif

class MainWindow;
class QAudioOutput;
class QMediaPlayer;
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
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	using Sound = QUrl;
#else
	using Sound = QMediaContent;
#endif

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
	Sound getSound(Event event);

	bool isPlayerAvailable();
	static bool isSoundValid(const Sound &sound);

	qint64 m_lastSoundMsec;
	QHash<int, Sound> m_sounds;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	QAudioOutput *m_audioOutput;
#endif
	QMediaPlayer *m_player;
};

}

#endif
