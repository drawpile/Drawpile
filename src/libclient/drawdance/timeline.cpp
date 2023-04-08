// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/timeline.h>
}

#include "libclient/drawdance/timeline.h"

namespace drawdance {

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

int Timeline::frameCount() const
{
    return DP_timeline_frame_count(m_data);
}

drawdance::Frame Timeline::frameAt(int i) const
{
    return drawdance::Frame::inc(DP_timeline_frame_at_noinc(m_data, i));
}

Timeline::Timeline(DP_Timeline *tl)
    : m_data{tl}
{
}

}
