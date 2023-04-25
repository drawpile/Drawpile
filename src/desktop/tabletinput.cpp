// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/tabletinput.h"
#include <QApplication>
#include <QSettings>

#ifdef Q_OS_WIN
#	include "bundled/kis_tablet/kis_tablet_support_win.h"
#	include "bundled/kis_tablet/kis_tablet_support_win8.h"
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// 		See qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
#		include <QtGui/private/qguiapplication_p.h>
#	endif
#endif

namespace tabletinput {

static const char *inputMode = "Qt tablet input";

#ifdef Q_OS_WIN

static Mode currentMode = Mode::Uninitialized;

class SpontaneousTabletEventFilter final : public QObject {
public:
	explicit SpontaneousTabletEventFilter(QObject *parent)
		: QObject{parent}
	{
	}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override final
	{
		switch(event->type()) {
		case QEvent::TabletEnterProximity:
		case QEvent::TabletLeaveProximity:
		case QEvent::TabletMove:
		case QEvent::TabletPress:
		case QEvent::TabletRelease:
			// KisTablet uses QApplication::sendEvent, which means its events
			// are never set to be spontaneous. We use that to filter out Qt's
			// own tablet events, which in turn are always spontaneous.
			if(event->spontaneous()) {
				return false;
			}
			Q_FALLTHROUGH();
		default:
			return QObject::eventFilter(watched, event);
		}
	}
};

static KisTabletSupportWin8 *kisTabletSupportWin8;
static SpontaneousTabletEventFilter *spontaneousTabletEventFilter;

static Mode getModeFromSettings(const QSettings &cfg)
{
	if(cfg.contains("settings/input/tabletinputmode")) {
		int value = cfg.value("settings/input/tabletinputmode").toInt();
		if(value > int(Mode::Uninitialized) && value <= int(Mode::Last)) {
			return Mode(value);
		} else {
			qWarning("Invalid settings/input/tabletinputmode: %d", value);
		}
	}
	// Drawpile 2.1 had separate Windows Ink and relative pen mode hack
	// settings, translate them if we don't find the new one.
	if(cfg.value("settings/input/windowsink", true).toBool()) {
		return Mode::KisTabletWinink;
	} else if(cfg.value("settings/input/relativepenhack", false).toBool()) {
		return Mode::KisTabletWintabRelativePenHack;
	} else {
		return Mode::KisTabletWintab;
	}
}

Mode extractMode(const QSettings &cfg)
{
	// Annoying fallback definitions to get the closest available mode.
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	constexpr Mode qt5 = Mode::Qt6Winink;
	constexpr Mode qt6Winink = Mode::Qt6Winink;
	constexpr Mode qt6Wintab = Mode::Qt6Wintab;
#	else
	constexpr Mode qt5 = Mode::Qt5;
	constexpr Mode qt6Winink = Mode::Qt5;
	constexpr Mode qt6Wintab = Mode::Qt5;
#	endif

	Mode mode = getModeFromSettings(cfg);
	switch(mode) {
	case Mode::Uninitialized:
		Q_UNREACHABLE();
	case Mode::KisTabletWinink:
	case Mode::KisTabletWintab:
	case Mode::KisTabletWintabRelativePenHack:
		return mode;
	case Mode::Qt5:
		return qt5;
	case Mode::Qt6Winink:
		return qt6Winink;
	case Mode::Qt6Wintab:
		return qt6Wintab;
	}
	Q_UNREACHABLE();
}

static void resetKisTablet(QApplication *app)
{
	if(spontaneousTabletEventFilter) {
		qApp->removeEventFilter(spontaneousTabletEventFilter);
		delete spontaneousTabletEventFilter;
		spontaneousTabletEventFilter = nullptr;
	}
	if(kisTabletSupportWin8) {
		app->removeNativeEventFilter(kisTabletSupportWin8);
		delete kisTabletSupportWin8;
		kisTabletSupportWin8 = nullptr;
	}
	KisTabletSupportWin::quit();
}

static void installSpontaneousTabletEventFilter(QApplication *app)
{
	spontaneousTabletEventFilter = new SpontaneousTabletEventFilter{app};
	app->installEventFilter(spontaneousTabletEventFilter);
}

static void enableKisTabletWinink(QApplication *app)
{
	kisTabletSupportWin8 = new KisTabletSupportWin8;
	if(kisTabletSupportWin8->init()) {
		installSpontaneousTabletEventFilter(app);
		app->installNativeEventFilter(kisTabletSupportWin8);
		inputMode = "KisTablet Windows Ink input";
		currentMode = Mode::KisTabletWinink;
	} else {
		qWarning("Failed to initialize KisTablet Windows Ink input");
		delete kisTabletSupportWin8;
		kisTabletSupportWin8 = nullptr;
	}
}

static void enableKisTabletWintab(QApplication *app, bool relativePenModeHack)
{
	if(KisTabletSupportWin::init()) {
		installSpontaneousTabletEventFilter(app);
		KisTabletSupportWin::enableRelativePenModeHack(relativePenModeHack);
		if(relativePenModeHack) {
			inputMode = "KisTablet Wintab input with relative pen mode hack";
		} else {
			inputMode = "KisTablet Wintab input";
		}
	} else {
		qWarning("Failed to initialize KisTablet Wintab input");
	}
}

#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
static void enableQt6TabletInput(QApplication *app, bool wintab)
{
	// See by qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
	using QWindowsApplication = QNativeInterface::Private::QWindowsApplication;
	QWindowsApplication *wa = app->nativeInterface<QWindowsApplication>();
	if(wa) {
		wa->setWinTabEnabled(wintab);
		if(wa->isWinTabEnabled()) {
			qDebug("Wintab enabled");
			inputMode = "Qt6 Wintab input";
		} else {
			qDebug("Wintab disabled");
			inputMode = "Qt6 Windows Ink input";
		}
	} else {
		qWarning("Error retrieving Windows platform integration");
	}
}
#	endif

static void resetQtInput(QApplication *app)
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

void update(QApplication *app, const QSettings &cfg)
{
#ifdef Q_OS_WIN
	Mode mode = extractMode(cfg);
	if(mode != currentMode) {
		resetKisTablet(app);
		resetQtInput(app);
		currentMode = Mode::Uninitialized;
		inputMode = "Invalid";
		switch(mode) {
		case Mode::KisTabletWinink:
			enableKisTabletWinink(app);
			break;
		case Mode::KisTabletWintab:
			enableKisTabletWintab(app, false);
			break;
		case Mode::KisTabletWintabRelativePenHack:
			enableKisTabletWintab(app, true);
			break;
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		case Mode::Qt6Winink:
			enableQt6TabletInput(app, false);
			break;
		case Mode::Qt6Wintab:
			enableQt6TabletInput(app, true);
			break;
#	else
		case Mode::Qt5:
			currentMode = Mode::Qt5;
			inputMode = "Qt5 tablet input";
			break;
#	endif
		default:
			break;
		}
	}
#else
	// Nothing to do on other platforms.
	Q_UNUSED(app);
	Q_UNUSED(cfg);
#endif
}

QString current()
{
	return QString::fromUtf8(inputMode);
}

#ifdef Q_OS_WIN
bool passPenEvents()
{
	// The spontaneous event filter is installed if and only if a KisTablet
	// input mode is currently active.
	return !spontaneousTabletEventFilter;
}
#endif

}
