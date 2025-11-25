// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/config/config.h"

namespace parentalcontrols {

void init(config::Config *cfg)
{
	g_cfg = cfg;
	// Dummy implementation -- no integration
}

bool isOSActive()
{
	// Dummy implementation -- no integration
	return false;
}

}
