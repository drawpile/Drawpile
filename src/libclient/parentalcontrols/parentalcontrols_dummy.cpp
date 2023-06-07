// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/settings.h"

namespace parentalcontrols {

void init(libclient::settings::Settings &settings)
{
	g_settings = &settings;
	// Dummy implementation -- no integration
}

bool isOSActive()
{
	// Dummy implementation -- no integration
	return false;
}

}
