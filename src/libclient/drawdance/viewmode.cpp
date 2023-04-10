// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include "dpengine/view_mode.h"
}

#include "libclient/drawdance/viewmode.h"

namespace drawdance {

ViewModeBuffer::ViewModeBuffer()
	: m_data{DP_view_mode_buffer_new()}
{
}

ViewModeBuffer::~ViewModeBuffer()
{
	DP_view_mode_buffer_free(m_data);
}

DP_ViewModeBuffer *ViewModeBuffer::get() const
{
	return m_data;
}

}
