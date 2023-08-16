// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_VIEWMODE_H
#define DRAWDANCE_VIEWMODE_H

extern "C" {
#include "dpengine/view_mode.h"
}

namespace drawdance {

class ViewModeBuffer final {
public:
	ViewModeBuffer();
	~ViewModeBuffer();

	ViewModeBuffer(const ViewModeBuffer &) = delete;
	ViewModeBuffer(ViewModeBuffer &&) = delete;
	ViewModeBuffer &operator=(const ViewModeBuffer &) = delete;
	ViewModeBuffer &operator=(ViewModeBuffer &&) = delete;

	DP_ViewModeBuffer *get();

private:
	DP_ViewModeBuffer m_data;
};

}

#endif
