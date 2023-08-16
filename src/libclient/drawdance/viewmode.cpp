// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/drawdance/viewmode.h"

namespace drawdance {

ViewModeBuffer::ViewModeBuffer()
{
	DP_view_mode_buffer_init(&m_data);
}

ViewModeBuffer::~ViewModeBuffer()
{
	DP_view_mode_buffer_dispose(&m_data);
}

DP_ViewModeBuffer *ViewModeBuffer::get()
{
	return &m_data;
}

}
