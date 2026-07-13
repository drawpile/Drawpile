// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_UTIL_ANDROIDUTILS_H
#define LIBSHARED_UTIL_ANDROIDUTILS_H
#include "libshared/util/qtcompat.h"
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#	include <QJniEnvironment>
#	include <QJniObject>
#else
#	include <QAndroidJniEnvironment>
#	include <QAndroidJniObject>
using QJniEnvironment = QAndroidJniEnvironment;
using QJniObject = QAndroidJniObject;
#endif

class QString;

namespace utils {

class AndroidExitInfo final {
public:
	// REASON_* constants from ApplicationExitInfo.
	enum class Reason {
		Unknown = 0,
		ExitSelf = 1,
		Signaled = 2,
		LowMemory = 3,
		Crash = 4,
		CrashNative = 5,
		Anr = 6,
		InitializationFailure = 7,
		PermissionChange = 8,
		ExcessiveResourceUsage = 9,
		UserRequested = 10,
		UserStopped = 11,
		DependencyDied = 12,
		Other = 13,
		Freezer = 14,
		PackageStateChange = 15,
		PackageUpdated = 16,
	};

	// IMPORTANCE_* constants from ActivityManager.RunningAppProcessInfo.
	enum class Importance {
		Foreground = 100,
		ForegroundService = 125,
		PerceptiblePre26 = 130,
		TopSleepingPre28 = 150,
		Visible = 200,
		Perceptible = 230,
		Service = 300,
		TopSleeping = 325,
		CantSaveState = 350,
		Cached = 400,
		Empty = 500,
		Gone = 1000,
	};

	AndroidExitInfo();
	AndroidExitInfo(
		int reasonCode, int exitOrSignalCode, int importanceCode,
		const QString &description);

	bool isValid() const { return m_valid; }
	int reasonCode() const { return m_reasonCode; }
	int exitOrSignalCode() const { return m_exitOrSignalCode; }
	int importanceCode() const { return m_importanceCode; }
	const QString &description() const { return m_description; }

	bool looksLikeCleanExit() const;
	bool looksLikeForegroundResourceExhaustionExit() const;

	QString buildLogString() const;

private:
	const QString m_description;
	const int m_reasonCode;
	const int m_exitOrSignalCode;
	const int m_importanceCode;
	const bool m_valid;
};

class AndroidWakeLock final {
public:
	AndroidWakeLock(const char *type, const QString &tag);
	~AndroidWakeLock();

	AndroidWakeLock(const AndroidWakeLock &) = delete;
	AndroidWakeLock(AndroidWakeLock &&) = delete;
	AndroidWakeLock &operator=(const AndroidWakeLock &) = delete;
	AndroidWakeLock &operator=(AndroidWakeLock &&) = delete;

private:
	QJniObject m_wakeLock;
};

class AndroidWifiLock final {
public:
	AndroidWifiLock(const char *type, const QString &tag);
	~AndroidWifiLock();

	AndroidWifiLock(const AndroidWifiLock &) = delete;
	AndroidWifiLock(AndroidWifiLock &&) = delete;
	AndroidWifiLock &operator=(const AndroidWifiLock &) = delete;
	AndroidWifiLock &operator=(AndroidWifiLock &&) = delete;

private:
	QJniObject m_wifiLock;
};

int androidLongPressTimeout();

bool androidLooksLikeXiaomiDevice();

bool androidIsLowMemoryKillReportSupported();

bool androidShowScalingDialog(
	double currentScale, double defaultScale, int interfaceMode,
	bool showOnStartup, bool canShowOnStartup);

bool androidShowForegroundResourceExhaustionWarningDialog();

#ifdef DRAWPILE_USE_CONNECT_SERVICE
bool createConnectionNotificationChannel();
bool shoulShowPostNotificationsRationale();
#endif

void startConnectService();
void stopConnectService();

AndroidExitInfo androidGetLastApplicationExitInfo();

QString androidGetContentUriBasename(const QString &contentUri);

}

#endif
