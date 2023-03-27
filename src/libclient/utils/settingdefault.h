// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SETTINGDEFAULT_H
#define SETTINGDEFAULT_H

#include <QtGlobal>

namespace setting_default {

// On most platforms, tablet input comes at a very high precision and frequency,
// so some smoothing is sensible by default. On Android (at least on a Samsung
// Galaxy S6 Lite, a Samsung Galaxy S8 Ultra and reports from unknown devices)
// the input is already pretty smooth though, so we'll leave it at zero there.
constexpr int smoothing()
{
#ifdef Q_OS_ANDROID
	return 0;
#else
	return 3;
#endif
}

}

#endif
