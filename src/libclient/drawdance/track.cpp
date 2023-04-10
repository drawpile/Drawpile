// SPDX-License-Identifier: GPL-3.0-or-later

extern "C" {
#include <dpengine/track.h>
}

#include "libclient/drawdance/track.h"
#include "libshared/util/qtcompat.h"

namespace drawdance {

Track Track::null()
{
	return Track{};
}

Track Track::inc(DP_Track *t)
{
	return Track{DP_track_incref_nullable(t)};
}

Track Track::noinc(DP_Track *t)
{
	return Track{t};
}

Track::Track(const Track &other)
	: Track{DP_track_incref_nullable(other.m_data)}
{
}

Track::Track(Track &&other)
	: Track{other.m_data}
{
	other.m_data = nullptr;
}

Track &Track::operator=(const Track &other)
{
	DP_track_decref_nullable(m_data);
	m_data = DP_track_incref_nullable(other.m_data);
	return *this;
}

Track &Track::operator=(Track &&other)
{
	DP_track_decref_nullable(m_data);
	m_data = other.m_data;
	other.m_data = nullptr;
	return *this;
}

Track::~Track()
{
	DP_track_decref_nullable(m_data);
}

bool Track::isNull() const
{
	return !m_data;
}

int Track::id() const
{
	return DP_track_id(m_data);
}

QString Track::title() const
{
	size_t length;
	const char *title = DP_track_title(m_data, &length);
	return QString::fromUtf8(title, compat::castSize(length));
}

bool Track::hidden() const
{
	return DP_track_hidden(m_data);
}

bool Track::onionSkin() const
{
	return DP_track_onion_skin(m_data);
}

int Track::keyFrameCount() const
{
	return DP_track_key_frame_count(m_data);
}

KeyFrame Track::keyFrameAt(int index) const
{
	return KeyFrame::inc(DP_track_key_frame_at_noinc(m_data, index));
}

int Track::frameIndexAt(int index) const
{
	return DP_track_frame_index_at_noinc(m_data, index);
}

Track::Track(DP_Track *t)
	: m_data{t}
{
}

}
