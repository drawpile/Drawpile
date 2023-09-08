// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H

#include <QApplication>
#include <QMap>
#include "desktop/notifications.h"
#include "desktop/settings.h"

class MainWindow;
class QSoundEffect;

namespace utils {
class Recents;
class StateDatabase;
}

class DrawpileApp final : public QApplication {
Q_OBJECT
	friend void notification::playSoundNow(notification::Event, int);
public:
	DrawpileApp(int &argc, char **argv);
	~DrawpileApp() override;

	void initState();

	void setThemeStyle(const QString &themeStyle);
	void setThemePalette(desktop::settings::ThemePalette themePalette);
	void initTheme();
	void initInterface();

	void openUrl(QUrl url);

	void openStart(const QString &page = QString{});

	void deleteAllMainWindowsExcept(MainWindow *win);

	const desktop::settings::Settings &settings() const { return m_settings; }
	desktop::settings::Settings &settings() { return m_settings; }

	const utils::StateDatabase &state() const { return *m_state; }
	utils::StateDatabase &state() { return *m_state; }

	const utils::Recents &recents() const { return *m_recents; }
	utils::Recents &recents() { return *m_recents; }

	bool smallScreenMode() const { return m_smallScreenMode; }

	// Returns a pair of (pixel size, physical size) of the primary screen.
	static QPair<QSize, QSizeF> screenResolution();

signals:
	void eraserNear(bool near);
	void setDockTitleBarsHidden(bool hidden);
	void focusCanvas();

protected:
	bool event(QEvent *e) override;

private:
	desktop::settings::Settings m_settings;
	bool m_smallScreenMode;
	utils::StateDatabase *m_state = nullptr;
	utils::Recents *m_recents = nullptr;
	QMap<notification::Event, QSoundEffect*> m_sounds;
	QString m_originalSystemStyle;
	QPalette m_originalSystemPalette;

	void updateThemeIcons();

	desktop::settings::InterfaceMode guessInterfaceMode();

	QPalette loadPalette(const QString &file);
};

inline DrawpileApp &dpApp()
{
	auto app = static_cast<DrawpileApp *>(QCoreApplication::instance());
	Q_ASSERT(app);
	return *app;
}

#endif
