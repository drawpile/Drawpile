/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "config.h"
#include "tabletinput.h"
#include <QApplication>
#include <QSettings>

#if defined(Q_OS_WIN)
#	if defined(KIS_TABLET)
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
#if defined(Q_OS_WIN) && defined(KIS_TABLET)
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

};
