// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FREEHAND_H
#define TOOLS_FREEHAND_H
#include "libclient/drawdance/strokeworker.h"
#include "libclient/tools/tool.h"
#include "libshared/net/message.h"
#include <QAtomicInt>
#include <QTimer>

struct DP_LayerContent;
struct DP_LayerGroup;
struct DP_LayerProps;
struct DP_MaskSync;
struct DP_Mutex;
struct DP_Semaphore;

namespace tools {

class SnapToPixelToggle {
public:
	virtual ~SnapToPixelToggle() = default;
	virtual void setSnapToPixel(bool snapToPixel) = 0;
};

class AntiOverflowSource final {
public:
	~AntiOverflowSource();
	DP_LayerContent *get(ToolController &owner);
	void clear();

private:
	DP_LayerContent *setLayerContentSource(DP_LayerContent *lc);
	DP_LayerContent *setLayerGroupSource(DP_LayerGroup *lg, DP_LayerProps *lp);

	DP_LayerContent *m_sourceLc = nullptr;
	DP_LayerGroup *m_sourceLg = nullptr;
};

class Freehand final : public Tool, public SnapToPixelToggle {
public:
	Freehand(ToolController &owner, DP_MaskSync *ms);
	~Freehand() override;

	void begin(const BeginParams &params) override;
	void beginStroke(const BeginParams &params, SnapToPixelToggle *target);
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	bool undoRedo(bool redo) override;

	bool usesBrushColor() const override { return true; }

	void offsetActiveTool(int x, int y) override;

	void setBrushSizeLimit(int limit) override;

	void setSelectionMaskingEnabled(bool selectionMaskingEnabled) override;

	void finish() override;
	void dispose() override;

	void setSnapToPixel(bool snapToPixel) override;

private:
	void strokeTo(const canvas::Point &point);
	void cancelStroke();
	void pushMessage(DP_Message *msg);
	void flushMessages();
	void pollControl(bool enable);
	void poll();
	DP_CanvasState *sync();
	void syncUnlock();
	static void syncUnlockCallback(void *user);

	void executePendingSyncMessages();
	static bool isPaintSyncInternalMessage(const net::Message &msg);

	static bool isOnMainThread();

	QTimer m_pollTimer;
	drawdance::StrokeWorker m_strokeWorker;
	AntiOverflowSource m_antiOverflowSource;
	DP_Mutex *m_mutex;
	DP_Semaphore *m_sem;
	net::MessageList m_messages;
	bool m_drawing = false;
	bool m_firstPoint = false;
	bool m_mirror = false;
	bool m_flip = false;
	canvas::Point m_start;
	qreal m_zoom = 1.0;
	qreal m_angle = 0.0;
	QAtomicInt m_cancelling = 0;
};

class FreehandEraser final : public Tool, public SnapToPixelToggle {
public:
	FreehandEraser(ToolController &owner, Freehand *freehand);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end(const EndParams &params) override;

	bool undoRedo(bool redo) override;

	bool usesBrushColor() const override { return true; }

	void offsetActiveTool(int x, int y) override;

	void finish() override;

	void setSnapToPixel(bool snapToPixel) override;

private:
	Freehand *const m_freehand;
};

}

#endif
