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

}

#endif
