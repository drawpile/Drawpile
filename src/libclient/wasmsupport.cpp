// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/wasmsupport.h"
#include "libclient/net/login.h"
#include <QString>
#include <emscripten.h>

#define DRAWPILE_BROWSER_AUTH_CANCEL -1
#define DRAWPILE_BROWSER_AUTH_USERNAME_SELECTED 1
#define DRAWPILE_BROWSER_AUTH_IDENTIFIED 2

static net::LoginHandler *currentLoginHandler;

namespace browser {

bool hasLowPressurePen()
{
	int lowPressurePen = EM_ASM_INT(return window.drawpileHasLowPressurePen(););
	return lowPressurePen != 0;
}

bool hasTroubleWithOpenGlCanvas()
{
	int trouble =
		EM_ASM_INT(return window.drawpileHasTroubleWithOpenGlCanvas(););
	return trouble != 0;
}

void showLoginModal(net::LoginHandler *loginHandler)
{
	cancelLoginModal(currentLoginHandler);
	currentLoginHandler = loginHandler;
	if(loginHandler) {
		EM_ASM(window.drawpileShowLoginModal(););
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
