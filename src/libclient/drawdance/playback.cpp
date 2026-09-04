// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/drawdance/playback.h"
#include "libclient/drawdance/global.h"
#include "libshared/net/message.h"

namespace drawdance {

Playback::~Playback()
{
	if(m_data) {
		freeData();
	}
}

void Playback::open()
{
	DP_DrawContext *dc;
	if(m_data) {
		dc = DP_playback_draw_context(m_data);
		DP_playback_free(m_data);
	} else {
		dc = drawdance::DrawContextPool::acquireRaw();
	}
	m_data = DP_playback_new(dc);
}

void Playback::close()
{
	if(m_data) {
		freeData();
		m_data = nullptr;
	}
}

void Playback::pushMessage(const net::Message &msg)
{
	DP_playback_push_message_inc(m_data, msg.get());
}

void Playback::freeData()
{
	DP_DrawContext *dc = DP_playback_draw_context(m_data);
	DP_playback_free(m_data);
	drawdance::DrawContextPool::releaseRaw(dc);
}

}
