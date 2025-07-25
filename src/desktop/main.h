// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H
#include <QApplication>
#include <QPalette>

class MainWindow;
class QCommandLineOption;
class QCommandLineParser;
class WinEventFilter;

namespace brushes {
class BrushPresetTagModel;
}

namespace desktop {
namespace settings {
class Settings;
}
}

namespace notification {
class Notifications;
}

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
	void setThemePalette(int themePalette);
	void initTheme();
	void initCanvasImplementation(const QString &arg);
	void initInterface();
	void initBrushPresets();

	void openPath(const QString &path, bool restoreWindowPosition);
	void joinUrl(
		const QUrl &url, const QString &autoRecordPath,
		bool restoreWindowPosition, bool singleSession);
	void openBlank(
		int width, int height, QColor backgroundColor,
		bool restoreWindowPosition);
	void openStart(const QString &page, bool restoreWindowPosition);

	void deleteAllMainWindowsExcept(MainWindow *win);

	const desktop::settings::Settings &settings() const { return *m_settings; }
	desktop::settings::Settings &settings() { return *m_settings; }

	notification::Notifications *notifications() { return m_notifications; }

	const utils::StateDatabase &state() const { return *m_state; }
	utils::StateDatabase &state() { return *m_state; }

	const utils::Recents &recents() const { return *m_recents; }
	utils::Recents &recents() { return *m_recents; }

	brushes::BrushPresetTagModel *brushPresets() { return m_brushPresets; }

	int canvasImplementation() const { return m_canvasImplementation; }
	static int getCanvasImplementationFor(int canvasImplementation);
	bool isCanvasImplementationFromSettings() const
	{
		return m_canvasImplementationFromSettings;
	}

	QSize safeNewCanvasSize() const;

	// Returns a pair of (pixel size, physical size) of the primary screen.
	static QPair<QSize, QSizeF> screenResolution();

	void setNewProcessArgs(
		const QCommandLineParser &parser,
		const QVector<const QCommandLineOption *> &options,
		const QVector<const QCommandLineOption *> &flags);

	// Runs a new Drawpile process with the given arguments. Returns if that
	// succeeded. Depending on the platform, this may always fail, e.g. Android
	// or the browser can't run stuff in new processes.
	bool runInNewProcess(const QStringList &args);

	// Checks if the given environment variable is set, non-empty and not set to
	// an integer value that evalues to zero.
	static bool isEnvTrue(const char *key);

signals:
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	void tabletProximityChanged(bool enter, bool eraser);
	void eraserNear(bool near);
#endif
	void focusCanvas();
	void shortcutsChanged();
	void tabletDriverChanged();

protected:
	bool event(QEvent *e) override;

private:
	desktop::settings::Settings *m_settings;
	notification::Notifications *m_notifications;
	int m_canvasImplementation;
	bool m_canvasImplementationFromSettings = false;
	utils::StateDatabase *m_state = nullptr;
	utils::Recents *m_recents = nullptr;
	brushes::BrushPresetTagModel *m_brushPresets = nullptr;
	QString m_originalSystemStyle;
	QPalette m_originalSystemPalette;
#ifdef Q_OS_WIN
	WinEventFilter *m_winEventFilter;
#endif
	bool m_wasEraserNear = false;
#ifdef HAVE_RUN_IN_NEW_PROCESS
	QStringList m_newProcessArgs;
#endif

	void updateThemeIcons();

	QPalette loadPalette(const QString &file);

	MainWindow *acquireWindow(bool restoreWindowPosition, bool singleSession);

#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	void updateEraserNear(bool near);
#endif
};

inline DrawpileApp &dpApp()
{
	DrawpileApp *app = static_cast<DrawpileApp *>(QCoreApplication::instance());
	Q_ASSERT(app);
	return *app;
}

#endif
