// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H

#include <QApplication>
#include <QMap>
#include "desktop/notifications.h"
#include "desktop/settings.h"

class MainWindow;
class QSoundEffect;

class DrawpileApp final : public QApplication {
Q_OBJECT
	friend void notification::playSoundNow(notification::Event, int);
public:
	DrawpileApp(int &argc, char **argv);
	~DrawpileApp() override;

	void setThemeStyle(const QString &themeStyle);
	void setThemePalette(desktop::settings::ThemePalette themePalette);
	void initTheme();

	void openUrl(QUrl url);

	void openBlankDocument();

	void deleteAllMainWindowsExcept(MainWindow *win);

	const desktop::settings::Settings &settings() const { return m_settings; }
	desktop::settings::Settings &settings() { return m_settings; }

signals:
	void eraserNear(bool near);
	void setDockTitleBarsHidden(bool hidden);

protected:
	bool event(QEvent *e) override;

private:
	desktop::settings::Settings m_settings;
	QMap<notification::Event, QSoundEffect*> m_sounds;
	void updateThemeIcons();

	QPalette loadPalette(const QString &file);
};

inline DrawpileApp &dpApp()
{
	auto app = static_cast<DrawpileApp *>(QCoreApplication::instance());
	Q_ASSERT(app);
	return *app;
}

#endif
