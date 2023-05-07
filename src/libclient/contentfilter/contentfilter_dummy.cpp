// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/contentfilter/contentfilter.h"
#include "libclient/settings.h"

namespace contentfilter {

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
