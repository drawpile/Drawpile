// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H
#include "desktop/notifications.h"
#include "desktop/settings.h"
#include <QApplication>
#include <QMap>
#ifdef Q_OS_WIN
#	include "desktop/utils/wineventfilter.h"
#endif

class MainWindow;
class QSoundEffect;

namespace utils {
class Recents;
class StateDatabase;
}

class DrawpileApp final : public QApplication {
	Q_OBJECT
public:
	DrawpileApp(int &argc, char **argv);
	~DrawpileApp() override;

	void initState();

	void setThemeStyle(const QString &themeStyle);
	void setThemePalette(desktop::settings::ThemePalette themePalette);
	void initTheme();
	void initCanvasImplementation(const QString &arg);
	void initInterface();

	void openPath(const QString &path);
	void joinUrl(const QUrl &url, bool singleSession);

	void openStart(const QString &page = QString{});

	void deleteAllMainWindowsExcept(MainWindow *win);

	const desktop::settings::Settings &settings() const { return m_settings; }
	desktop::settings::Settings &settings() { return m_settings; }

	notification::Notifications *notifications() { return m_notifications; }

	const utils::StateDatabase &state() const { return *m_state; }
	utils::StateDatabase &state() { return *m_state; }

	const utils::Recents &recents() const { return *m_recents; }
	utils::Recents &recents() { return *m_recents; }

	int canvasImplementation() const { return m_canvasImplementation; }
	bool canvasImplementationUsesTileCache();
	static int getCanvasImplementationFor(int canvasImplementation);

	bool smallScreenMode() const { return m_smallScreenMode; }

	// Returns a pair of (pixel size, physical size) of the primary screen.
	static QPair<QSize, QSizeF> screenResolution();

signals:
#ifndef __EMSCRIPTEN__
	void tabletProximityChanged(bool enter, bool eraser);
	void eraserNear(bool near);
#endif
	void setDockTitleBarsHidden(bool hidden);
	void focusCanvas();

protected:
	bool event(QEvent *e) override;

private:
	desktop::settings::Settings m_settings;
	notification::Notifications *m_notifications;
	int m_canvasImplementation;
	bool m_smallScreenMode;
	utils::StateDatabase *m_state = nullptr;
	utils::Recents *m_recents = nullptr;
	QString m_originalSystemStyle;
	QPalette m_originalSystemPalette;
#ifdef Q_OS_WIN
	WinEventFilter winEventFilter;
#endif
	bool m_wasEraserNear = false;

	void updateThemeIcons();

	desktop::settings::InterfaceMode guessInterfaceMode();

	QPalette loadPalette(const QString &file);

	MainWindow *acquireWindow(bool singleSession);

#ifndef __EMSCRIPTEN__
	void updateEraserNear(bool near);
#endif
};

inline DrawpileApp &dpApp()
{
	auto app = static_cast<DrawpileApp *>(QCoreApplication::instance());
	Q_ASSERT(app);
	return *app;
}

#endif
