// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/util/androidutils.h"
#include <QString>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#	include <QtAndroid>
#endif

namespace utils {

static bool clearException(QJniEnvironment &env)
{
	if(env->ExceptionCheck()) {
		qWarning("JNI exception occurred");
		env->ExceptionDescribe();
		env->ExceptionClear();
		return true;
	} else {
		return false;
	}
}

static bool checkValid(const char *name, QJniObject &obj)
{
	if(obj.isValid()) {
		return true;
	} else {
		qWarning("JNI object '%s' is not valid", name);
		return false;
	}
}

static QJniObject acquireLock(
	const char *what, const char *type, const QString &tag, const char *service,
	const char *managerClass, const char *newFn, const char *newFnSignature)
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return QJniObject{};
	}

	QJniObject serviceName = QJniObject::getStaticObjectField<jstring>(
		"android/content/Context", service);
	if(clearException(env) || !checkValid("serviceName", serviceName)) {
		return QJniObject{};
	}

	QJniObject manager = activity.callObjectMethod(
		"getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
		serviceName.object<jobject>());
	if(clearException(env) || !checkValid("manager", manager)) {
		return QJniObject{};
	}

	jint flags = QJniObject::getStaticField<jint>(managerClass, type);
	if(clearException(env)) {
		return QJniObject{};
	}

	QJniObject tagObj = QJniObject::fromString(tag);
	if(clearException(env) || !checkValid("tagObject", tagObj)) {
		return QJniObject{};
	}

	QJniObject lock = manager.callObjectMethod(
		newFn, newFnSignature, flags, tagObj.object<jstring>());
	if(clearException(env) || !checkValid("lock", lock)) {
		return QJniObject{};
	}

	lock.callMethod<void>("acquire", "()V");
	if(clearException(env)) {
		return QJniObject{};
	}

	qDebug("Acquired %s lock", what);
	return lock;
}

static void releaseLock(const char *what, QJniObject &lock)
{
	QJniEnvironment env;
	if(lock.isValid()) {
		lock.callMethod<void>("release", "()V");
		if(clearException(env)) {
			qWarning("Error releasing %s lock", what);
		} else {
			qDebug("Released %s lock", what);
		}
	}
}


static QJniObject acquireWakeLock(const char *type, const QString &tag)
{
	return acquireLock(
		"wake", type, tag, "POWER_SERVICE", "android/os/PowerManager",
		"newWakeLock",
		"(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
}

static void releaseWakeLock(QJniObject &wakeLock)
{
	releaseLock("wake", wakeLock);
}

AndroidWakeLock::AndroidWakeLock(const char *type, const QString &tag)
	: m_wakeLock{acquireWakeLock(type, tag)}
{
}

AndroidWakeLock::~AndroidWakeLock()
{
	releaseWakeLock(m_wakeLock);
}


static QJniObject acquireWifiLock(const char *type, const QString &tag)
{
	return acquireLock(
		"wifi", type, tag, "WIFI_SERVICE", "android/net/wifi/WifiManager",
		"createWifiLock",
		"(ILjava/lang/String;)Landroid/net/wifi/WifiManager$WifiLock;");
}

static void releaseWifiLock(QJniObject &wifiLock)
{
	releaseLock("wifi", wifiLock);
}

AndroidWifiLock::AndroidWifiLock(const char *type, const QString &tag)
	: m_wifiLock{acquireWifiLock(type, tag)}
{
}

AndroidWifiLock::~AndroidWifiLock()
{
	releaseWifiLock(m_wifiLock);
}


int androidLongPressTimeout()
{
	QJniEnvironment env;
	jint value = QJniObject::callStaticMethod<jint>(
		"net/drawpile/android/MainActivity", "getLongPressTimeout", "()I");
	if(clearException(env) || value <= 0) {
		return 500;
	} else {
		return int(value);
	}
}

bool androidLooksLikeXiaomiDevice()
{
	static int looksLikeXiaomiDeviceResult;
	if(looksLikeXiaomiDeviceResult == 0) {
		QJniEnvironment env;
		jboolean value = QJniObject::callStaticMethod<jboolean>(
			"net/drawpile/android/MainActivity", "looksLikeXiaomiDevice", "()Z");
		if(clearException(env) || !value) {
			looksLikeXiaomiDeviceResult = -1;
		} else {
			looksLikeXiaomiDeviceResult = 1;
		}
	}
	return looksLikeXiaomiDeviceResult > 0;
}

#ifdef DRAWPILE_USE_CONNECT_SERVICE
bool createConnectionNotificationChannel()
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return false;
	}

	jboolean created = activity.callMethod<jboolean>(
		"createConnectionNotificationChannel", "()Z");
	if(clearException(env)) {
		return false;
	}

	activity.callMethod<void>("showTestConnectionNotification", "()V");
	if(clearException(env)) {
		return false;
	}

	return created != 0;
}

bool shoulShowPostNotificationsRationale()
{
	QString permission =
		QStringLiteral("android.permission.POST_NOTIFICATIONS");
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#		error "shoulShowPostNotificationsRationale not implemented for Qt6"
#	else
	return QtAndroid::shouldShowRequestPermissionRationale(permission);
#	endif
}
#endif

void startConnectService()
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(!clearException(env) && checkValid("activity", activity)) {
		activity.callMethod<void>("startConnectService", "()V");
		clearException(env);
	}
}

void stopConnectService()
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(!clearException(env) && checkValid("activity", activity)) {
		activity.callMethod<void>("stopConnectService", "()V");
		clearException(env);
	}
}

}
