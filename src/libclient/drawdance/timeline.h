// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DRAWDANCE_TIMELINE_H
#define DRAWDANCE_TIMELINE_H

#include "libclient/drawdance/track.h"

struct DP_Timeline;

namespace drawdance {

class Timeline final {
public:
	static Timeline null();
	static Timeline inc(DP_Timeline *tl);
	static Timeline noinc(DP_Timeline *tl);

	Timeline() {}
	Timeline(const Timeline &other);
	Timeline(Timeline &&other);

	Timeline &operator=(const Timeline &other);
	Timeline &operator=(Timeline &&other);

	~Timeline();

	bool isNull() const;

	int trackCount() const;

	Track trackAt(int index) const;

private:
	explicit Timeline(DP_Timeline *tl);

	DP_Timeline *m_data;
};

}

#endif
