// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_VIEWMODE_H
#define DRAWDANCE_VIEWMODE_H

struct DP_ViewModeBuffer;

namespace drawdance {

class ViewModeBuffer final {
public:
	ViewModeBuffer();
	~ViewModeBuffer();

	ViewModeBuffer(const ViewModeBuffer &) = delete;
	ViewModeBuffer(ViewModeBuffer &&) = delete;
	ViewModeBuffer &operator=(const ViewModeBuffer &) = delete;
	ViewModeBuffer &operator=(ViewModeBuffer &&) = delete;

	DP_ViewModeBuffer *get() const;

private:
	DP_ViewModeBuffer *m_data;
};

}

#endif
