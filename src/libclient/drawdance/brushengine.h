// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DRAWDANCE_BRUSH_ENGINE_H
#define DRAWDANCE_BRUSH_ENGINE_H
extern "C" {
#include <dpengine/brush_engine.h>
}
#include "libclient/net/message.h"
#include <functional>

struct DP_ClassicBrush;
struct DP_MyPaintBrush;
struct DP_MyPaintSettings;

namespace canvas {
class Point;
}

namespace net {
class Client;
}

namespace drawdance {

class CanvasState;

class BrushEngine final {
public:
	using PollControlFn = std::function<void(bool)>;

	BrushEngine(PollControlFn pollControl = nullptr);
	~BrushEngine();

	BrushEngine(const BrushEngine &) = delete;
	BrushEngine(BrushEngine &&) = delete;
	BrushEngine &operator=(const BrushEngine &) = delete;
	BrushEngine &operator=(BrushEngine &&) = delete;

	void setClassicBrush(
		const DP_ClassicBrush &brush, const DP_StrokeParams &stroke);

	void setMyPaintBrush(
		const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
		const DP_StrokeParams &stroke);

	void flushDabs();

	const net::MessageList &messages() const { return m_messages; }

	void clearMessages() { m_messages.clear(); }

	void beginStroke(unsigned int contextId, bool pushUndoPoint, float zoom);

	void strokeTo(const canvas::Point &point, const drawdance::CanvasState &cs);

	void poll(long long timeMsec, const drawdance::CanvasState &cs);

	void endStroke(
		long long timeMsec, const drawdance::CanvasState &cs, bool pushPenUp);

	void addOffset(float x, float y);

	// Flushes dabs and sends accumulated messages to the client.
	void sendMessagesTo(net::Client *client);

private:
	static void pushMessage(void *user, DP_Message *msg);
	static void pollControl(void *user, bool enable);

	net::MessageList m_messages;
	PollControlFn m_pollControl;
	DP_BrushEngine *m_data;
};

}

#endif
