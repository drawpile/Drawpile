// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/main.h"
#include "desktop/tabletinput.h"

#include <QCoreApplication>

#ifdef Q_OS_WIN
#	include "bundled/kis_tablet/kis_tablet_support_win.h"
#	include "bundled/kis_tablet/kis_tablet_support_win8.h"
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// 		See qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
#		include <QtGui/private/qguiapplication_p.h>
#	endif
#endif

namespace tabletinput {

static Mode currentMode = Mode::Uninitialized;

#ifdef Q_OS_WIN

static KisTabletSupportWin8 *kisTabletSupportWin8;
static bool shouldPassPenEvents = true;

static void resetKisTablet(DrawpileApp &app)
{
	shouldPassPenEvents = true;
	if(kisTabletSupportWin8) {
		app.removeNativeEventFilter(kisTabletSupportWin8);
		delete kisTabletSupportWin8;
		kisTabletSupportWin8 = nullptr;
	}
	KisTabletSupportWin::quit();
}

static void enableKisTabletWinink(DrawpileApp &app, bool nativePositions)
{
	kisTabletSupportWin8 = new KisTabletSupportWin8;
	if(kisTabletSupportWin8->init(nativePositions)) {
		shouldPassPenEvents = false;
		app.installNativeEventFilter(kisTabletSupportWin8);
		currentMode = Mode::KisTabletWinink;
	} else {
		qWarning("Failed to initialize KisTablet Windows Ink input");
		delete kisTabletSupportWin8;
		kisTabletSupportWin8 = nullptr;
	}
}

static void enableKisTabletWintab(bool relativePenModeHack)
{
	if(KisTabletSupportWin::init()) {
		shouldPassPenEvents = false;
		KisTabletSupportWin::enableRelativePenModeHack(relativePenModeHack);
		if(relativePenModeHack) {
			currentMode = Mode::KisTabletWintabRelativePenHack;
		} else {
			currentMode = Mode::KisTabletWintab;
		}
	} else {
		qWarning("Failed to initialize KisTablet Wintab input");
	}
}

#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
static void enableQt6TabletInput(DrawpileApp &app, bool wintab)
{
	// See by qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
	using QWindowsApplication = QNativeInterface::Private::QWindowsApplication;
	if (auto *wa = app.nativeInterface<QWindowsApplication>()) {
		wa->setWinTabEnabled(wintab);
		if(wa->isWinTabEnabled()) {
			qDebug("Wintab enabled");
			currentMode = Mode::Qt6Wintab;
		} else {
			qDebug("Wintab disabled");
			currentMode = Mode::Qt6Winink;
		}
	} else {
		qWarning("Error retrieving Windows platform integration");
	}
}
#	endif

static void resetQtInput(DrawpileApp &app)
{
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// At the time of writing, enabling Wintab mode in Qt6 causes menus to react
	// to pen presses only very sporadically. Enabling KisTablet Wintab mode
	// while Qt6 is in Windows Ink mode works fine though, so we reset to that.
	enableQt6TabletInput(app, false);
#	else
	Q_UNUSED(app);
#	endif
}

#endif

void init(DrawpileApp &app)
{
#ifdef Q_OS_WIN
	app.settings().bindTabletDriver([&](Mode mode) {
		resetKisTablet(app);
		resetQtInput(app);
		currentMode = Mode::Uninitialized;
		switch(mode) {
		case Mode::KisTabletWinink:
			enableKisTabletWinink(app, true);
			break;
		case Mode::KisTabletWininkNonNative:
			enableKisTabletWinink(app, false);
			break;
		case Mode::KisTabletWintab:
			enableKisTabletWintab(false);
			break;
		case Mode::KisTabletWintabRelativePenHack:
			enableKisTabletWintab(true);
			break;
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		case Mode::Qt5:
		case Mode::Qt6Winink:
			enableQt6TabletInput(app, false);
			break;
		case Mode::Qt6Wintab:
			enableQt6TabletInput(app, true);
			break;
#	else
		case Mode::Qt5:
		case Mode::Qt6Winink:
		case Mode::Qt6Wintab:
			currentMode = Mode::Qt5;
			break;
#	endif
		case Mode::Uninitialized:
			break;
		}
		emit app.tabletDriverChanged();
	});
#else
	// Nothing to do on other platforms.
	Q_UNUSED(app);
#endif
}

const char *current()
{
	switch(currentMode) {
	case Mode::Uninitialized:
	case Mode::Qt5:
		return QT_TRANSLATE_NOOP("tabletinput", "Qt tablet input");
	case Mode::KisTabletWinink:
		return QT_TRANSLATE_NOOP("tabletinput", "KisTablet Windows Ink input");
	case Mode::KisTabletWininkNonNative:
		return QT_TRANSLATE_NOOP(
			"tabletinput", "KisTablet Windows Ink non-native input");
	case Mode::KisTabletWintab:
		return QT_TRANSLATE_NOOP("tabletinput", "KisTablet Wintab input");
	case Mode::KisTabletWintabRelativePenHack:
		return QT_TRANSLATE_NOOP(
			"tabletinput", "KisTablet Wintab input with relative pen mode");
	case Mode::Qt6Winink:
		return QT_TRANSLATE_NOOP("tabletinput", "Qt6 Windows Ink input");
	case Mode::Qt6Wintab:
		return QT_TRANSLATE_NOOP("tabletinput", "Qt6 Wintab input");
	}
	Q_UNREACHABLE();
}

#ifdef Q_OS_WIN
bool passPenEvents()
{
	return shouldPassPenEvents;
}
#endif

}
