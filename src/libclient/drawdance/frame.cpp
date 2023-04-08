// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/frame.h>
}

#include "libclient/drawdance/frame.h"

namespace drawdance {

Frame Frame::inc(DP_Frame *f)
{
    return Frame{DP_frame_incref_nullable(f)};
}

Frame Frame::noinc(DP_Frame *f)
{
    return Frame{f};
}

Frame::Frame(const Frame &other)
    : Frame{DP_frame_incref_nullable(other.m_data)}
{
}

Frame::Frame(Frame &&other)
    : Frame{other.m_data}
{
    other.m_data = nullptr;
}

Frame &Frame::operator=(const Frame &other)
{
    DP_frame_decref_nullable(m_data);
    m_data = DP_frame_incref_nullable(other.m_data);
    return *this;
}

Frame &Frame::operator=(Frame &&other)
{
    DP_frame_decref_nullable(m_data);
    m_data = other.m_data;
    other.m_data = nullptr;
    return *this;
}

Frame::~Frame()
{
    DP_frame_decref_nullable(m_data);
}

bool Frame::isNull() const
{
    return !m_data;
}

int Frame::layerIdCount() const
{
    return DP_frame_layer_id_count(m_data);
}

int Frame::layerIdAt(int index) const
{
    return DP_frame_layer_id_at(m_data, index);
}

Frame::Frame(DP_Frame *f)
    : m_data{f}
{
}

}
