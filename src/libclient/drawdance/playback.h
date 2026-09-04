// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_PLAYBACK_H
#define LIBCLIENT_DRAWDANCE_PLAYBACK_H
extern "C" {
#include <dpengine/playback.h>
}
#include "libclient/drawdance/canvasstate.h"

struct DP_DrawContext;

namespace net {
class Message;
}

namespace drawdance {

class Playback final {
public:
	Playback() = default;
	~Playback();

	Playback(const Playback &) = delete;
	Playback(Playback &&) = delete;
	Playback &operator=(const Playback &) = delete;
	Playback &operator=(Playback &&) = delete;

	DP_Playback *get() { return m_data; }
	DP_DrawContext *drawContext() { return DP_playback_draw_context(m_data); }

	void open();
	void close();

	void pushMessage(const net::Message &msg);

	void flushMultidab() { DP_playback_flush_multidab(m_data); }

	drawdance::CanvasState localCanvasState() const
	{
		return drawdance::CanvasState::noinc(
			DP_playback_local_canvas_inc(m_data));
	}

private:
	void freeData();

	DP_Playback *m_data = nullptr;
};

}

#endif
