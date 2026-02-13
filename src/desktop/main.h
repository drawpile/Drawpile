// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWPILEAPP_H
#define DRAWPILEAPP_H
#include <QApplication>
#include <QPair>
#include <QPalette>
#include <QVector>
#ifdef Q_OS_ANDROID
#	include <QScreen>
#endif

class GlobalKeyEventFilter;
class LongPressEventFilter;
class MainWindow;
class QCommandLineOption;
class QCommandLineParser;
class QSettings;
class WinEventFilter;

namespace brushes {
class BrushPresetTagModel;
}

namespace config {
class Config;
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
	void setThemePalette(const QString &themePalette);
	void initTheme();
	void initCanvasImplementation(const QString &arg);
	void initInterface();
	void initBrushPresets();

	void openPath(const QString &path, bool restoreWindowPosition);
	void joinUrl(
		const QUrl &url, const QString &autoRecordPath, int connectStrategy,
		bool restoreWindowPosition, bool singleSession);
	void openBlank(
		int width, int height, QColor backgroundColor,
		bool restoreWindowPosition);
	MainWindow *openDefault(bool restoreWindowPosition);
	void newDefaultDocument(MainWindow *win);
	void openStart(const QString &page, bool restoreWindowPosition);

	void deleteAllMainWindowsExcept(MainWindow *win);

	config::Config *config() { return m_config; }
	QSettings *scalingSettings();
	void resetSettingsPath(const QString &path);

	notification::Notifications *notifications() { return m_notifications; }

	const utils::StateDatabase &state() const { return *m_state; }
	utils::StateDatabase &state() { return *m_state; }

	const utils::Recents &recents() const { return *m_recents; }
	utils::Recents &recents() { return *m_recents; }

	brushes::BrushPresetTagModel *brushPresets() { return m_brushPresets; }

	void setLanguage(const QString &language) { m_language = language; }
	const QString language() const { return m_language; }

	int canvasImplementation() const { return m_canvasImplementation; }
	static int getCanvasImplementationFor(int canvasImplementation);
	bool isCanvasImplementationFromSettings() const
	{
		return m_canvasImplementationFromSettings;
	}

	QSize safeNewCanvasSize() const;

	bool anyTabletEventsReceived() const;

	// Returns a pair of (pixel size, physical size) of the primary screen.
	static QPair<QSize, QSizeF> screenResolution();

	void setNewProcessArgs(
		const QCommandLineParser &parser,
		const QVector<const QCommandLineOption *> &options,
		const QVector<const QCommandLineOption *> &flags);

	// Runs a new Drawpile process with the given arguments. Returns if that
	// succeeded. Depending on the platform, this may always fail, e.g. Android
	// or the browser can't run stuff in new processes.
	bool runInNewProcess(
		const QStringList &args,
		const QVector<QPair<QString, QString>> &envVars = {});

	// Checks if the given environment variable is set, non-empty and not set to
	// an integer value that evalues to zero.
	static bool isEnvTrue(const char *key);

	static bool isAndroidScalingDialogShown();
	static bool takeAndroidScalingJustChanged();

#if defined(Q_OS_ANDROID) && defined(KRITA_QT_SCREEN_DENSITY_ADJUSTMENT)
	void showAndroidScalingDialog();
	void handleAndroidScalingDialogDismissed();
#endif

signals:
	void tabletEventReceived();
#if !defined(__EMSCRIPTEN__) && !defined(Q_OS_ANDROID)
	void tabletProximityChanged(bool enter, bool eraser);
	void eraserNear(bool near);
#endif
	void focusCanvas();
	void shortcutsChanged();
	void tabletDriverChanged();
	void refreshApplicationStyleRequested();
	void refreshApplicationFontRequested();
#if defined(Q_OS_ANDROID) && defined(KRITA_QT_SCREEN_DENSITY_ADJUSTMENT)
	// These may be emitted from the Android UI thread. Connect to them with
	// Qt::QueuedConnection to avoid getting hit by them asynchronously.
	void androidScalingDialogShown();
	void androidScalingDialogDismissed();
#endif

protected:
	bool event(QEvent *e) override;

private:
	desktop::settings::Settings *m_settings;
	config::Config *m_config;
	notification::Notifications *m_notifications;
	int m_canvasImplementation;
	bool m_canvasImplementationFromSettings = false;
	utils::StateDatabase *m_state = nullptr;
	utils::Recents *m_recents = nullptr;
	brushes::BrushPresetTagModel *m_brushPresets = nullptr;
	QString m_language;
	QString m_originalSystemStyle;
	QPalette m_originalSystemPalette;
	GlobalKeyEventFilter *m_globalEventFilter;
#ifdef Q_OS_WIN
	WinEventFilter *m_winEventFilter;
#endif
	LongPressEventFilter *m_longPressEventFilter = nullptr;
#ifdef HAVE_RUN_IN_NEW_PROCESS
	QStringList m_newProcessArgs;
#endif

	void updateThemeIcons();

	MainWindow *acquireWindow(bool restoreWindowPosition, bool singleSession);

	void setLongPressEnabled(bool enabled);
};

static inline DrawpileApp &dpApp()
{
	DrawpileApp *app = static_cast<DrawpileApp *>(QCoreApplication::instance());
	Q_ASSERT(app);
	return *app;
}

static inline config::Config *dpAppConfig()
{
	return dpApp().config();
}

#endif
