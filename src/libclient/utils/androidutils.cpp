// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/utils/androidutils.h"
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QRegularExpression>
#include <QString>
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#	include <QtAndroid>
#endif

Q_LOGGING_CATEGORY(lcDpAndroidUtils, "net.drawpile.androidutils", QtWarningMsg)

namespace utils {

AndroidExitInfo::AndroidExitInfo()
	: m_reasonCode(int(Reason::Unknown))
	, m_exitOrSignalCode(-1)
	, m_importanceCode(-1)
	, m_valid(false)
{
}

AndroidExitInfo::AndroidExitInfo(
	int reasonCode, int exitOrSignalCode, int importanceCode,
	const QString &description)
	: m_description(description)
	, m_reasonCode(reasonCode)
	, m_exitOrSignalCode(exitOrSignalCode)
	, m_importanceCode(importanceCode)
	, m_valid(true)
{
}

bool AndroidExitInfo::looksLikeCleanExit() const
{
	switch(m_reasonCode) {
	case int(Reason::ExitSelf):
	case int(Reason::UserRequested):
	case int(Reason::UserStopped):
	case int(Reason::PackageStateChange):
	case int(Reason::PackageUpdated):
		return true;
	default:
		return false;
	}
}

bool AndroidExitInfo::looksLikeForegroundResourceExhaustionExit() const
{
	if(m_importanceCode == int(Importance::Foreground)) {
		switch(m_reasonCode) {
		case int(Reason::LowMemory):
		case int(Reason::ExcessiveResourceUsage):
			return true;
		case int(Reason::Signaled):
			// Not all devices support reporting OOM kills and the Android
			// documentation says that they will report a SIGKILL instead.
			return !androidIsLowMemoryKillReportSupported() &&
				   m_exitOrSignalCode == 9;
		default:
			break;
		}
	}
	return false;
}

QString AndroidExitInfo::buildLogString() const
{
	if(!isValid()) {
		return QString();
	}

	QString message;

	message.append(QStringLiteral("reason %1").arg(m_reasonCode));
	switch(m_reasonCode) {
	case int(Reason::Unknown):
		message.append(QStringLiteral(" (UNKNOWN)"));
		break;
	case int(Reason::ExitSelf):
		message.append(QStringLiteral(" (EXIT_SELF)"));
		break;
	case int(Reason::Signaled):
		message.append(QStringLiteral(" (SIGNALED)"));
		break;
	case int(Reason::LowMemory):
		message.append(QStringLiteral(" (LOW_MEMORY)"));
		break;
	case int(Reason::Crash):
		message.append(QStringLiteral(" (CRASH)"));
		break;
	case int(Reason::CrashNative):
		message.append(QStringLiteral(" (CRASH_NATIVE)"));
		break;
	case int(Reason::Anr):
		message.append(QStringLiteral(" (ANR)"));
		break;
	case int(Reason::InitializationFailure):
		message.append(QStringLiteral(" (INITIALIZATION_FAILURE)"));
		break;
	case int(Reason::PermissionChange):
		message.append(QStringLiteral(" (PERMISSION_CHANGE)"));
		break;
	case int(Reason::ExcessiveResourceUsage):
		message.append(QStringLiteral(" (EXCESSIVE_RESOURCE_USAGE)"));
		break;
	case int(Reason::UserRequested):
		message.append(QStringLiteral(" (USER_REQUESTED)"));
		break;
	case int(Reason::UserStopped):
		message.append(QStringLiteral(" (USER_STOPPED)"));
		break;
	case int(Reason::DependencyDied):
		message.append(QStringLiteral(" (DEPENDENCY_DIED)"));
		break;
	case int(Reason::Other):
		message.append(QStringLiteral(" (OTHER)"));
		break;
	case int(Reason::Freezer):
		message.append(QStringLiteral(" (FREEZER)"));
		break;
	case int(Reason::PackageStateChange):
		message.append(QStringLiteral(" (PACKAGE_STATE_CHANGE)"));
		break;
	case int(Reason::PackageUpdated):
		message.append(QStringLiteral(" (PACKAGE_UPDATED)"));
		break;
	default:
		break;
	}

	message.append(QStringLiteral(", importance %1").arg(m_importanceCode));
	switch(m_importanceCode) {
	case int(Importance::Foreground):
		message.append(QStringLiteral(" (FOREGROUND)"));
		break;
	case int(Importance::ForegroundService):
		message.append(QStringLiteral(" (FOREGROUND_SERVICE)"));
		break;
	case int(Importance::PerceptiblePre26):
		message.append(QStringLiteral(" (PERCEPTIBLE_PRE_26)"));
		break;
	case int(Importance::TopSleepingPre28):
		message.append(QStringLiteral(" (TOP_SLEEPING_PRE_28)"));
		break;
	case int(Importance::Visible):
		message.append(QStringLiteral(" (VISIBLE)"));
		break;
	case int(Importance::Perceptible):
		message.append(QStringLiteral(" (PERCEPTIBLE)"));
		break;
	case int(Importance::Service):
		message.append(QStringLiteral(" (SERVICE)"));
		break;
	case int(Importance::TopSleeping):
		message.append(QStringLiteral(" (TOP_SLEEPING)"));
		break;
	case int(Importance::CantSaveState):
		message.append(QStringLiteral(" (CANT_SAVE_STATE)"));
		break;
	case int(Importance::Cached):
		message.append(QStringLiteral(" (CACHED)"));
		break;
	case int(Importance::Empty):
		message.append(QStringLiteral(" (EMPTY)"));
		break;
	case int(Importance::Gone):
		message.append(QStringLiteral(" (GONE)"));
		break;
	default:
		break;
	}

	message.append(
		QStringLiteral(", exit code/signal %1").arg(m_exitOrSignalCode));

	message.append(QStringLiteral(", low-memory kill report "));
	if(androidIsLowMemoryKillReportSupported()) {
		message.append(QStringLiteral("supported"));
	} else {
		message.append(QStringLiteral("not supported"));
	}

	if(looksLikeCleanExit()) {
		message.append(QStringLiteral(", looks"));
	} else {
		message.append(QStringLiteral(", does not look"));
	}
	message.append(QStringLiteral(" like clean exit"));

	if(looksLikeForegroundResourceExhaustionExit()) {
		message.append(QStringLiteral(", looks"));
	} else {
		message.append(QStringLiteral(", does not look"));
	}
	message.append(QStringLiteral(" like foreground resource exhaustion exit"));

	if(m_description.isEmpty()) {
		message.append(QStringLiteral(", no description"));
	} else {
		message.append(QStringLiteral(", description '%1'").arg(m_description));
	}

	return message;
}


static bool clearException(QJniEnvironment &env)
{
	if(env->ExceptionCheck()) {
		qCWarning(lcDpAndroidUtils, "JNI exception occurred");
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
		qCWarning(lcDpAndroidUtils, "JNI object '%s' is not valid", name);
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

	qCDebug(lcDpAndroidUtils, "Acquired %s lock", what);
	return lock;
}

static void releaseLock(const char *what, QJniObject &lock)
{
	QJniEnvironment env;
	if(lock.isValid()) {
		lock.callMethod<void>("release", "()V");
		if(clearException(env)) {
			qCWarning(lcDpAndroidUtils, "Error releasing %s lock", what);
		} else {
			qCDebug(lcDpAndroidUtils, "Released %s lock", what);
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
			"net/drawpile/android/MainActivity", "looksLikeXiaomiDevice",
			"()Z");
		if(clearException(env) || !value) {
			looksLikeXiaomiDeviceResult = -1;
		} else {
			looksLikeXiaomiDeviceResult = 1;
		}
	}
	return looksLikeXiaomiDeviceResult > 0;
}

bool androidIsLowMemoryKillReportSupported()
{
	static int lowMemoryKillReportSupportedResult;
	if(lowMemoryKillReportSupportedResult == 0) {
		QJniEnvironment env;
		jboolean value = QJniObject::callStaticMethod<jboolean>(
			"net/drawpile/android/MainActivity",
			"isLowMemoryKillReportSupported", "()Z");
		if(clearException(env) || !value) {
			lowMemoryKillReportSupportedResult = -1;
		} else {
			lowMemoryKillReportSupportedResult = 1;
		}
	}
	return lowMemoryKillReportSupportedResult > 0;
}

bool androidShowScalingDialog(
	double currentScale, double defaultScale, int interfaceMode,
	bool showOnStartup, bool canShowOnStartup)
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return false;
	}

	QJniObject percentTextFormat = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidScalingDialog", "Interface scale: %1"));
	QJniObject percentDefaultTextFormat = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidScalingDialog", "Interface scale: %1 (default)"));
	QJniObject dynamicText = QJniObject::fromString(
		QCoreApplication::translate(
			"dialogs::settingsdialog::UserInterface", "Dynamic"));
	QJniObject desktopText = QJniObject::fromString(
		QCoreApplication::translate(
			"dialogs::settingsdialog::UserInterface", "Desktop"));
	QJniObject mobileText = QJniObject::fromString(
		QCoreApplication::translate(
			"dialogs::settingsdialog::UserInterface", "Mobile"));
	QJniObject rememberCheckBoxText = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidScalingDialog", "Show this dialog when Drawpile starts"));
	QJniObject okButtonText = QJniObject::fromString(
		QCoreApplication::translate("AndroidScalingDialog", "OK"));

	activity.callMethod<void>(
		"showScalingDialog",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/"
		"String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;DDIZZ)V",
		percentTextFormat.object<jstring>(),
		percentDefaultTextFormat.object<jstring>(),
		dynamicText.object<jstring>(), desktopText.object<jstring>(),
		mobileText.object<jstring>(), rememberCheckBoxText.object<jstring>(),
		okButtonText.object<jstring>(), jdouble(currentScale),
		jdouble(defaultScale), jint(interfaceMode), jboolean(showOnStartup),
		jboolean(canShowOnStartup));
	if(clearException(env)) {
		return false;
	}

	return true;
}

bool androidShowForegroundResourceExhaustionWarningDialog()
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return false;
	}

	QJniObject titleText = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidForegroundResourceExhaustionWarningDialog",
			"Resource Exhaustion"));
	QJniObject messageText = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidForegroundResourceExhaustionWarningDialog",
			"It looks like Drawpile exited because it ran out of memory. If "
			"you were trying to join a session or open a file, its canvas may "
			"be too large or have too many layers for your device to handle."));
	QJniObject buttonText = QJniObject::fromString(
		QCoreApplication::translate(
			"AndroidForegroundResourceExhaustionWarningDialog", "OK"));

	activity.callMethod<void>(
		"showForegroundResourceExhaustionWarningDialog",
		"(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
		titleText.object<jstring>(), messageText.object<jstring>(),
		buttonText.object<jstring>());
	if(clearException(env)) {
		return false;
	}

	return true;
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

AndroidExitInfo androidGetLastApplicationExitInfo()
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return AndroidExitInfo();
	}

	QJniObject exitInfo = activity.callObjectMethod(
		"getLastApplicationExitInfo", "()Landroid/app/ApplicationExitInfo;");
	if(clearException(env) || !checkValid("exitInfo", exitInfo)) {
		return AndroidExitInfo();
	}

	int reasonCode = exitInfo.callMethod<jint>("getReason", "()I");
	if(clearException(env)) {
		return AndroidExitInfo();
	}

	int exitOrSignalCode = exitInfo.callMethod<jint>("getStatus", "()I");
	if(clearException(env)) {
		exitOrSignalCode = -1;
	}

	int importanceCode = exitInfo.callMethod<jint>("getImportance", "()I");
	if(clearException(env)) {
		importanceCode = -1;
	}

	QString description;
	{
		QJniObject descriptionObject =
			exitInfo.callObjectMethod("getDescription", "()Ljava/lang/String;");
		if(!clearException(env) && descriptionObject.isValid()) {
			description = descriptionObject.toString();
		}
	}

	return AndroidExitInfo(
		reasonCode, exitOrSignalCode, importanceCode, description);
}

static QString getFallbackContentUriBasename(const QString &contentUri)
{
	// Find the last colon (%3A) or slash (%2F) and grab whatever comes after.
	static QRegularExpression re(
		QStringLiteral("(?<=:|/|%3a|%2f)"),
		QRegularExpression::CaseInsensitiveOption);
	int i = contentUri.lastIndexOf(re);
	if(i != -1 && i < contentUri.length() - 1) {
		return contentUri.mid(i);
	} else {
		return contentUri;
	}
}

static QString getContentUriDisplayName(const QString &contentUri)
{
	QJniEnvironment env;
	QJniObject activity = QJniObject::callStaticObjectMethod(
		"org/qtproject/qt5/android/QtNative", "activity",
		"()Landroid/app/Activity;");
	if(clearException(env) || !checkValid("activity", activity)) {
		return QString();
	}

	QJniObject contentUriObj = QJniObject::fromString(contentUri);
	if(clearException(env) || !checkValid("contentUriObj", contentUriObj)) {
		return QString();
	}

	QJniObject basenameObj = activity.callObjectMethod(
		"getContentUriBasename", "(Ljava/lang/String;)Ljava/lang/String;",
		contentUriObj.object<jstring>());
	if(clearException(env) || !checkValid("basenameObj", basenameObj)) {
		return QString();
	}

	QString basename = basenameObj.toString();
	clearException(env);
	return basename;
}

QString androidGetContentUriBasename(const QString &contentUri)
{
	if(contentUri.isEmpty()) {
		return QString();
	}

	QString displayName = getContentUriDisplayName(contentUri);
	if(!displayName.isEmpty()) {
		return displayName;
	}

	return getFallbackContentUriBasename(contentUri);
}

}
