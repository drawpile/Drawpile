// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"
#include "libclient/config/config.h"

#include <wpcapi.h>

#include <QDebug>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcDpParentalControlsWin, "net.drawpile.parentalcontrols.win", QtWarningMsg)

namespace parentalcontrols {

static const CLSID CLSID_WinParentalControls = {0xE77CC89B,0x7401,0x4C04,{0x8C,0xED,0x14,0x9D,0xB3,0x5A,0xDD,0x04}};
static const IID IID_IWinParentalControls  = {0x28B4D88B,0xE072,0x49E6,{0x80,0x4D,0x26,0xED,0xBE,0x21,0xA7,0xB9}};

static bool ACTIVE = false;

void init(config::Config *cfg)
{
	g_cfg = cfg;
	qCDebug(lcDpParentalControlsWin, "Initializing parental controls");

	CoInitialize(nullptr);
	HRESULT hr;
	IWindowsParentalControls *pc;
	hr = CoCreateInstance(CLSID_WinParentalControls, nullptr, CLSCTX_INPROC, IID_IWinParentalControls, (void**)&pc);

	if(FAILED(hr)) {
		qCDebug(lcDpParentalControlsWin, "parental controls failed");
		return;
	}

	IWPCSettings *wpcs;
	hr = pc->GetUserSettings(nullptr, &wpcs);
	if(hr == S_OK) {
		DWORD restrictions;
		wpcs->GetRestrictions(&restrictions);
		if((restrictions & WPCFLAG_WEB_FILTERED)) {
			ACTIVE = true;
		}
		wpcs->Release();
	}
	pc->Release();

	qCDebug(lcDpParentalControlsWin, "parental controls: %s", ACTIVE?"active":"not enabled");
}

bool isOSActive()
{
	return ACTIVE;
}

}
