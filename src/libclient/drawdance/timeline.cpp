// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/timeline.h>
}

#include "libclient/drawdance/timeline.h"

namespace drawdance {

Timeline Timeline::null()
{
	return Timeline{};
}

Timeline Timeline::inc(DP_Timeline *tl)
{
	return Timeline{DP_timeline_incref_nullable(tl)};
}

Timeline Timeline::noinc(DP_Timeline *tl)
{
	return Timeline{tl};
}

Timeline::Timeline(const Timeline &other)
	: Timeline{DP_timeline_incref_nullable(other.m_data)}
{
}

Timeline::Timeline(Timeline &&other)
	: Timeline{other.m_data}
{
	other.m_data = nullptr;
}

Timeline &Timeline::operator=(const Timeline &other)
{
	DP_timeline_decref_nullable(m_data);
	m_data = DP_timeline_incref_nullable(other.m_data);
	return *this;
}

Timeline &Timeline::operator=(Timeline &&other)
{
	DP_timeline_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Timeline::~Timeline()
{
	DP_timeline_decref_nullable(m_data);
}

bool Timeline::isNull() const
{
	return !m_data;
}

int Timeline::trackCount() const
{
	return DP_timeline_count(m_data);
}

Track Timeline::trackAt(int index) const
{
	return Track::inc(DP_timeline_at_noinc(m_data, index));
}

Timeline::Timeline(DP_Timeline *tl)
	: m_data{tl}
{
}

}
