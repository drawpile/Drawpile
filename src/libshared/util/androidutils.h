#ifndef ANDROIDUTILS_H
#define ANDROIDUTILS_H

#include <QAndroidJniObject>

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
	QAndroidJniObject m_wakeLock;
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
	QAndroidJniObject m_wifiLock;
};

}

#endif
