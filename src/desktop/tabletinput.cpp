// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/tabletinput.h"
#include <QApplication>
#include <QSettings>

#if defined(Q_OS_WIN)
#	if defined(HAVE_KIS_TABLET)
#		include "bundled/kis_tablet/kis_tablet_support_win.h"
#		include "bundled/kis_tablet/kis_tablet_support_win8.h"
#	elif QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
// 		See qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
#		include <QtGui/private/qguiapplication_p.h>
#		include <QtGui/qpa/qplatformintegration.h>
#	endif
#endif

namespace tabletinput {

static const char *inputMode = "Qt tablet input";

void init(QApplication *app, const QSettings &cfg)
{
#if defined(Q_OS_WIN) && defined(HAVE_KIS_TABLET)
	bool useWindowsInk = false;
	// Enable Windows Ink tablet event handler
	// This was taken directly from Krita
	if(cfg.value("settings/input/windowsink", true).toBool()) {
		KisTabletSupportWin8 *penFilter = new KisTabletSupportWin8();
		if(penFilter->init()) {
			app->installNativeEventFilter(penFilter);
			useWindowsInk = true;
			inputMode = "KisTablet Windows Ink input";
			qDebug("Using Win8 Pointer Input for tablet support");

		} else {
			qWarning("No Win8 Pointer Input available");
			delete penFilter;
		}
	} else {
		qDebug("Win8 Pointer Input disabled");
	}

	if(!useWindowsInk) {
		// Enable modified Wintab support
		// This too was taken from Krita
		qDebug("Enabling custom Wintab support");
		KisTabletSupportWin::init();
		if(cfg.value("settings/input/relativepenhack", false).toBool()) {
			KisTabletSupportWin::enableRelativePenModeHack(true);
			inputMode = "KisTablet Wintab input with relative pen mode hack";
		} else {
			KisTabletSupportWin::enableRelativePenModeHack(false);
			inputMode = "KisTablet Wintab input";
		}
	}
#elif defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	Q_UNUSED(app);
	update(cfg);
#else
	// Nothing to do on other platforms.
	Q_UNUSED(app);
	Q_UNUSED(cfg);
#endif
}

void update(const QSettings &cfg)
{
#if defined(Q_OS_WIN) && QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	// See by qtbase tests/manual/qtabletevent/regular_widgets/main.cpp
	using QWindowsApplication = QNativeInterface::Private::QWindowsApplication;
	QWindowsApplication *wa = dynamic_cast<QWindowsApplication *>(
		QGuiApplicationPrivate::platformIntegration());
	if(wa) {
        bool windowsInk = cfg.value("settings/input/windowsink", true).toBool();
		wa->setWinTabEnabled(!windowsInk);
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
#else
	// Nothing to do on other platforms, KIS_TABLET needs a restart.
	Q_UNUSED(cfg);
#endif
}

QString current()
{
	return QString::fromUtf8(inputMode);
}

}
