// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_DRAWDANCE_STROKEWORKER_H
#define LIBCLIENT_DRAWDANCE_STROKEWORKER_H
#include <functional>

struct DP_BrushEngineStrokeParams;
struct DP_CanvasState;
struct DP_ClassicBrush;
struct DP_MaskSync;
struct DP_Message;
struct DP_MyPaintBrush;
struct DP_MyPaintSettings;
struct DP_StrokeWorker;

namespace canvas {
class Point;
}

namespace drawdance {

class CanvasState;

class StrokeWorker final {
public:
	using PushMessageFn = std::function<void(DP_Message *msg)>;
	using PollControlFn = std::function<void(bool)>;
	using SyncFn = std::function<DP_CanvasState *()>;

	StrokeWorker(
		DP_MaskSync *msOrNull, const PushMessageFn &pushMessage,
		const PollControlFn &pollControl, const SyncFn &sync);
	~StrokeWorker();

	StrokeWorker(const StrokeWorker &) = delete;
	StrokeWorker(StrokeWorker &&) = delete;
	StrokeWorker &operator=(const StrokeWorker &) = delete;
	StrokeWorker &operator=(StrokeWorker &&) = delete;

	bool isThreadActive() const;
	bool startThread();
	bool finishThread();

	void setSizeLimit(int sizeLimit);

	void setClassicBrush(
		const DP_ClassicBrush &brush, const DP_BrushEngineStrokeParams &besp,
		bool eraserOverride);

	void setMyPaintBrush(
		const DP_MyPaintBrush &brush, const DP_MyPaintSettings &settings,
		const DP_BrushEngineStrokeParams &besp, bool eraserOverride);

	void flushDabs();

	void pushMessageNoinc(DP_Message *msg);

	void beginStroke(
		unsigned int contextId, const drawdance::CanvasState &cs,
		bool compatibilityMode, bool pushUndoPoint, bool mirror, bool flip,
		float zoom, float angle);

	void strokeTo(const canvas::Point &point, const drawdance::CanvasState &cs);

	void poll(long long timeMsec, const drawdance::CanvasState &cs);

	void endStroke(
		long long timeMsec, const drawdance::CanvasState &cs, bool pushPenUp);

	void cancelStroke(long long timeMsec, bool pushPenUp);

	void addOffset(float x, float y);

private:
	static void pushMessage(void *user, DP_Message *msg);
	static void pollControl(void *user, bool enable);
	static DP_CanvasState *sync(void *user);

	PushMessageFn m_pushMessage;
	PollControlFn m_pollControl;
	SyncFn m_sync;
	DP_StrokeWorker *m_data;
};

}

#endif
