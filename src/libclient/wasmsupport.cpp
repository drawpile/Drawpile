// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/wasmsupport.h"
#include "libclient/net/login.h"
#include <QCoreApplication>
#include <QHostAddress>
#include <QUrl>
#include <emscripten.h>

#define DRAWPILE_BROWSER_AUTH_CANCEL -1
#define DRAWPILE_BROWSER_AUTH_USERNAME_SELECTED 1
#define DRAWPILE_BROWSER_AUTH_IDENTIFIED 2

static net::LoginHandler *currentLoginHandler;
static bool persistentFileSystemSyncInProgress;

extern "C" void drawpileFinishPersistentFileSystemSync()
{
	persistentFileSystemSyncInProgress = false;
}

EM_JS(void, drawpileMountPersistentFileSystem, (int argc, char **argv), {
	try {
		console.log("Loading persistent file system");
		FS.mkdir("/appdata");
		FS.mount(IDBFS, {}, "/appdata");
		FS.syncfs(
			true, function(err) {
				if(err) {
					console.error("Error loading persistent file system:", err);
				} else {
					console.log("Persistent file system loaded");
					_drawpileInitScheduleSyncPersistentFileSystem();
				}
				setTimeout(_drawpileMain.bind(null, argc, argv), 0);
			});
	} catch(e) {
		console.error("Error mounting persistent file system:", e);
		setTimeout(_drawpileMain.bind(null, argc, argv), 0);
	}
})

EM_JS(void, drawpileSyncPersistentFileSystem, (), {
	try {
		FS.syncfs(
			false, function(err) {
				if(err) {
					console.error("Error persisting file system:", err);
				}
				_drawpileFinishPersistentFileSystemSync();
			});
	} catch(e) {
		console.error("Error trying to persist file system:", err);
		_drawpileFinishPersistentFileSystemSync();
	}
})

namespace browser {

bool hasLowPressurePen()
{
	int lowPressurePen = EM_ASM_INT(return window.drawpileHasLowPressurePen(););
	return lowPressurePen != 0;
}

namespace {
QString convertAndFreeWasmString(char *value)
{
	if(value) {
		QString s = QString::fromUtf8(value);
		free(value);
		return s;
	} else {
		return QString();
	}
}
}

QString getLocale()
{
	char *locale = static_cast<char *>(
		EM_ASM_PTR(return stringToNewUTF8(window.drawpileLocale || "");));
	QString s = convertAndFreeWasmString(locale).trimmed();
	return s.isEmpty() ? QStringLiteral("en_US") : s;
}

QString getWelcomeMessage()
{
	char *message = static_cast<char *>(EM_ASM_PTR(
		return stringToNewUTF8(window.drawpileStandaloneMessage || "");));
	return convertAndFreeWasmString(message).trimmed();
}

QString getHostaddressParam()
{
	char *value = static_cast<char *>(EM_ASM_PTR(return stringToNewUTF8(
		new URLSearchParams(window.location.search).get("hostaddress") || "")));
	return convertAndFreeWasmString(value).trimmed();
}

QString getHostpassParam()
{
	char *value = static_cast<char *>(EM_ASM_PTR(return stringToNewUTF8(
		new URLSearchParams(window.location.search).get("hostpass") || "")));
	return convertAndFreeWasmString(value).trimmed();
}

QString getUsernameParam()
{
	char *value = static_cast<char *>(EM_ASM_PTR(return stringToNewUTF8(
		new URLSearchParams(window.location.search).get("username") || "")));
	return convertAndFreeWasmString(value).trimmed();
}

QString getUserpassParam()
{
	char *value = static_cast<char *>(EM_ASM_PTR(return stringToNewUTF8(
		new URLSearchParams(window.location.search).get("userpass") || "")));
	return convertAndFreeWasmString(value).trimmed();
}

void showLoginModal(net::LoginHandler *loginHandler, const QString &username)
{
	cancelLoginModal(currentLoginHandler);
	currentLoginHandler = loginHandler;
	if(loginHandler) {
		if(username.isEmpty()) {
			EM_ASM(window.drawpileShowLoginModal(););
		} else {
			QByteArray usernameBytes = username.toUtf8();
			EM_ASM(window.drawpileShowLoginModal($0, $1);
				   , usernameBytes.constData(), int(usernameBytes.size()));
		}
	}
}

void cancelLoginModal(net::LoginHandler *loginHandler)
{
	if(loginHandler && currentLoginHandler == loginHandler) {
		currentLoginHandler = nullptr;
		EM_ASM(window.drawpileCancelLoginModal(););
	}
}

void authenticate(net::LoginHandler *loginHandler, const QByteArray &payload)
{
	if(loginHandler == currentLoginHandler) {
		EM_ASM(window.drawpileAuthenticate($0, $1);
			   , payload.constData(), int(payload.size()));
	}
}

static QString guessFailedConnectionReason(const QUrl &url)
{
	// clang-format off
	bool isOnWebDrawpileNet =
		EM_ASM_INT({ return window.location.host === "web.drawpile.net"; });
	// clang-format on
	if(!isOnWebDrawpileNet) {
		return QCoreApplication::translate(
			"wasmsupport",
			"You're not using the official client on web.drawpile.net. Most "
			"servers do not allow connections from elsewhere.");
	}

	if(url.scheme().compare(QStringLiteral("wss"), Qt::CaseInsensitive) != 0) {
		return QCoreApplication::translate(
			"wasmsupport",
			"The session address does not look like a valid WebSocket URL.");
	}

	if(QHostAddress().setAddress(url.host())) {
		return QCoreApplication::translate(
			"wasmsupport",
			"You're trying to connect to an IP address instead of a proper "
			"domain name. This usually doesn't work unless you've configured "
			"your browser to allow this first.");
	}

	return QCoreApplication::translate(
		"wasmsupport", "The server may not support joining via web browser.");
}

void intuitFailedConnectionReason(QString &description, const QUrl &url)
{
	QString guess = guessFailedConnectionReason(url);
	if(!guess.isEmpty()) {
		description.append(QStringLiteral("\n\n"));
		description.append(guess);
	}
}

void mountPersistentFileSystem(int argc, char **argv)
{
	drawpileMountPersistentFileSystem(argc, argv);
}

bool syncPersistentFileSystem()
{
	if(persistentFileSystemSyncInProgress) {
		return false;
	} else {
		persistentFileSystemSyncInProgress = true;
		drawpileSyncPersistentFileSystem();
		return true;
	}
}

bool isFullscreenSupported()
{
	return EM_ASM_INT(return window.document.fullscreenEnabled ? 1 : 0;);
}

bool isFullscreen()
{
	return EM_ASM_INT(return window.document.fullscreenElement ? 1 : 0;);
}

void toggleFullscreen()
{
	if(isFullscreen()) {
		EM_ASM(window.document.exitFullscreen(););
	} else {
		EM_ASM(window.document.body.requestFullscreen(););
	}
}

}

// Stuff called from JavaScript.

extern "C" void drawpileHandleBrowserAuth(int type, char *rawArg)
{
	QString arg;
	if(rawArg) {
		arg = QString::fromUtf8(rawArg);
		free(rawArg);
	}

	if(currentLoginHandler) {
		switch(type) {
		case DRAWPILE_BROWSER_AUTH_CANCEL:
			currentLoginHandler->cancelBrowserAuth();
			break;
		case DRAWPILE_BROWSER_AUTH_USERNAME_SELECTED:
			currentLoginHandler->selectBrowserAuthUsername(arg);
			break;
		case DRAWPILE_BROWSER_AUTH_IDENTIFIED:
			currentLoginHandler->browserAuthIdentified(arg);
			break;
		default:
			qWarning(
				"Unknown browser auth type %d with argument '%s'", type,
				qUtf8Printable(arg));
			break;
		}
	} else {
		qWarning(
			"No login handler set to handle auth type %d with argument '%s'",
			type, qUtf8Printable(arg));
	}
}

// Text to translate
#ifdef DRAWPILE_TRANSLATION_ONLY_JUST_HERE_FOR_LUPDATE
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"Memory allocation failed. This can happen in some browsers if you "
	"refresh. Close this page and your browser entirely, then try again.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"Memory allocated is not a SharedArrayBuffer, even though your browser "
	"supports it. You may need to change a setting.")
QT_TRANSLATE_NOOP("BrowserStartPage", "Drawpile logo")
QT_TRANSLATE_NOOP("BrowserStartPage", "drawpile.net login")
QT_TRANSLATE_NOOP("BrowserStartPage", "Loading application")
QT_TRANSLATE_NOOP("BrowserStartPage", "Starting up…")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "Starting, this should only take a moment…")
QT_TRANSLATE_NOOP("BrowserStartPage", "Loading assets")
QT_TRANSLATE_NOOP("BrowserStartPage", "Loading, this may take a while…")
QT_TRANSLATE_NOOP("BrowserStartPage", "Preparing")
QT_TRANSLATE_NOOP("BrowserStartPage", "Initializing, this may take a while…")
QT_TRANSLATE_NOOP("BrowserStartPage", "Failed to start Drawpile: %1")
QT_TRANSLATE_NOOP("BrowserStartPage", "Setting up…")
QT_TRANSLATE_NOOP("BrowserStartPage", "Fatal error:")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"This is usually because your browser is outdated or you are viewing the "
	"page through some kind of embedded browser, like a chat application's, "
	"and need to open it properly. For more information, check out <a "
	"href=\"#\">this help page</a>.")
QT_TRANSLATE_NOOP("BrowserStartPage", "Invalid session link.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"However you got here was not via a valid link to a Drawpile session.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "<strong>In-app browser:</strong> it looks like you "
						"opened Drawpile in an in-app browser.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "<strong>In-app browser:</strong> it looks like you "
						"opened Drawpile in %1's in-app browser.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "That usually doesn't work properly, please open this "
						"page in a real web browser instead.")
QT_TRANSLATE_NOOP("BrowserStartPage", "Possibly incompatible browser:")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"<strong>Possibly incompatible browser:</strong> Firefox on Linux doesn't "
	"have support for pressure-sensitive pens on all systems. If you don't get "
	"pressure, consider using a different browser or <a href=\"#\">the native "
	"Linux application</a>.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"<strong>Incompatible browser:</strong> Firefox on Windows has some "
	"trouble running Drawpile. Inputting text and/or pressing Ctrl+Z to undo "
	"may not work properly. Consider using a different browser or <a "
	"href=\"#\">the native Windows application</a>.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"<strong>Incompatible browser:</strong> Chrome on %1 has some trouble "
	"running Drawpile. It's really just the system browser (Safari) in a "
	"different shell, which causes problems with controls ending up "
	"off-screen. Consider using the system browser (Safari) directly instead.")
QT_TRANSLATE_NOOP("BrowserStartPage", "Language:")
QT_TRANSLATE_NOOP("BrowserStartPage", "Test your pen pressure here")
QT_TRANSLATE_NOOP("BrowserStartPage", "Detected mouse input, not a pen")
QT_TRANSLATE_NOOP("BrowserStartPage", "Detected touch input, not a pen")
QT_TRANSLATE_NOOP("BrowserStartPage", "Detected input, but not a pen")
QT_TRANSLATE_NOOP("BrowserStartPage", "Pen pressure detected")
QT_TRANSLATE_NOOP("BrowserStartPage", "Pen detected, but no pressure variance")
QT_TRANSLATE_NOOP("BrowserStartPage", "Checking for updates…")
QT_TRANSLATE_NOOP("BrowserStartPage", "Update check blocked")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "<strong>Warning:</strong> this installation is "
						"<strong>outdated</strong> at version <code>%1</code>, "
						"which is not the most recent version <code>%2</code>.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage", "Try refreshing the page. If that doesn't change "
						"anything, notify the server owner to update.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"<strong>Warning:</strong> Could not determine if installation is up to "
	"date or not. It may be outdated at version <code>%1</code>.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"Try refreshing the page. If that doesn't change anything, consult the "
	"server owner or check out <a href=\"#\">the help page on drawpile.net</a> "
	"on how to get in contact with someone who can check what's going on.")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"You can continue regardless, but things may not work properly.")
QT_TRANSLATE_NOOP("BrowserStartPage", "Start")
QT_TRANSLATE_NOOP("BrowserStartPage", "Start Anyway")
QT_TRANSLATE_NOOP("BrowserStartPage", "Version:")
QT_TRANSLATE_NOOP("BrowserStartPage", "Stable (%1)")
QT_TRANSLATE_NOOP("BrowserStartPage", "Beta (%1)")
QT_TRANSLATE_NOOP(
	"BrowserStartPage",
	"The session you are trying to join requires the beta version of Drawpile.")
#endif
