// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/parentalcontrols/parentalcontrols.h"

#include <wpcapi.h>

#include <QDebug>

namespace parentalcontrols {

static const CLSID CLSID_WinParentalControls = {0xE77CC89B,0x7401,0x4C04,{0x8C,0xED,0x14,0x9D,0xB3,0x5A,0xDD,0x04}};
static const IID IID_IWinParentalControls  = {0x28B4D88B,0xE072,0x49E6,{0x80,0x4D,0x26,0xED,0xBE,0x21,0xA7,0xB9}};

static bool ACTIVE = false;

void init()
{
	qDebug("Initializing parental controls");

	CoInitialize(nullptr);
	HRESULT hr;
	IWindowsParentalControls *pc;
	hr = CoCreateInstance(CLSID_WinParentalControls, nullptr, CLSCTX_INPROC, IID_IWinParentalControls, (void**)&pc);

	if(FAILED(hr)) {
		qDebug("parental controls failed");
		return;
	}

	IWPCSettings *wpcs;
	hr = pc->GetUserSettings(nullptr, &wpcs);
	if(hr == S_OK) {
		DWORD settings;
		wpcs->GetRestrictions(&settings);
		// If web filtering is enabled, active PC mode
		if((settings & 0x02)) {
			ACTIVE = true;
		}
		wpcs->Release();
	}
	pc->Release();

	qDebug("parental controls: %s", ACTIVE?"active":"not enabled");
}

bool isOSActive()
{
	return ACTIVE;
}

}
