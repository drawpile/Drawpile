#include "androidutils.h"
#include <QAndroidJniEnvironment>
#include <QAndroidJniObject>
#include <QString>

namespace utils {

static bool clearException(QAndroidJniEnvironment &env)
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

static bool checkValid(const char *name, QAndroidJniObject &obj)
{
	if(obj.isValid()) {
		return true;
	} else {
		qWarning("JNI object '%s' is not valid", name);
		return false;
	}
}

static QAndroidJniObject acquireLock(
	const char *what, const char *type, const QString &tag, const char *service,
	const char *managerClass, const char *newFn, const char *newFnSignature)
{
	QAndroidJniEnvironment env;
	QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return QAndroidJniObject{};
	}

	QAndroidJniObject serviceName =
		QAndroidJniObject::getStaticObjectField<jstring>(
			"android/content/Context", service);
	if(clearException(env) || !checkValid("serviceName", serviceName)) {
		return QAndroidJniObject{};
	}

	QAndroidJniObject manager = activity.callObjectMethod(
		"getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;",
		serviceName.object<jobject>());
	if(clearException(env) || !checkValid("manager", manager)) {
		return QAndroidJniObject{};
	}

	jint flags = QAndroidJniObject::getStaticField<jint>(managerClass, type);
	if(clearException(env)) {
		return QAndroidJniObject{};
	}

	QAndroidJniObject tagObj = QAndroidJniObject::fromString(tag);
	if(clearException(env) || !checkValid("tagObject", tagObj)) {
		return QAndroidJniObject{};
	}

	QAndroidJniObject lock = manager.callObjectMethod(
		newFn, newFnSignature, flags, tagObj.object<jstring>());
	if(clearException(env) || !checkValid("lock", lock)) {
		return QAndroidJniObject{};
	}

	lock.callMethod<void>("acquire", "()V");
	if(clearException(env)) {
		return QAndroidJniObject{};
	}

	qDebug("Acquired %s lock", what);
	return lock;
}

static void releaseLock(const char *what, QAndroidJniObject &lock)
{
	QAndroidJniEnvironment env;
	if(lock.isValid()) {
		lock.callMethod<void>("release", "()V");
		if(clearException(env)) {
			qWarning("Error releasing %s lock", what);
		} else {
			qDebug("Released %s lock", what);
		}
	}
}


static QAndroidJniObject acquireWakeLock(const char *type, const QString &tag)
{
	return acquireLock(
		"wake", type, tag, "POWER_SERVICE", "android/os/PowerManager",
		"newWakeLock",
		"(ILjava/lang/String;)Landroid/os/PowerManager$WakeLock;");
}

static void releaseWakeLock(QAndroidJniObject &wakeLock)
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


static QAndroidJniObject acquireWifiLock(const char *type, const QString &tag)
{
	return acquireLock(
		"wifi", type, tag, "WIFI_SERVICE", "android/net/wifi/WifiManager",
		"createWifiLock",
		"(ILjava/lang/String;)Landroid/net/wifi/WifiManager$WifiLock;");
}

static void releaseWifiLock(QAndroidJniObject &wifiLock)
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


}
