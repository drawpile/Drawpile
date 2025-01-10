// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_MAGICWAND_H
#define LIBCLIENT_TOOLS_MAGICWAND_H
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"
#include <QImage>
#include <QPoint>

namespace tools {

class MagicWandTool final : public Tool {
public:
	MagicWandTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void modify(const ModifyParams &params) override;
	void hover(const HoverParams &params) override;
	void end(const EndParams &params) override;
	bool isMultipart() const override;
	void finishMultipart() override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;

	void updateParameters();

private:
	// Dummy constant for potential future editable fill functionality, which
	// used to previously exist, similar to how the flood fill tool works
	// currently. However, this is less necessary with selections being
	// local-only, so currently editable wand selections are always disabled.
	static constexpr bool EDITABLE = false;

	class Task;
	friend Task;

	void updateOp(bool constrain, bool center, int defaultOp);
	void updateCursor(bool constrain, bool center);
	const QCursor &getCursor(int effectiveOp) const;
	void stopDragging();
	void fillAt(const QPointF &point, bool constrain, bool center);
	void repeatFill();
	void floodFillFinished(Task *task);

	bool havePending() const { return !m_pendingImage.isNull(); }
	void updateToolNotice();
	void flushPending();
	void disposePending();

	void adjustPendingImage();

	int m_op = -1;
	bool m_running = false;
	bool m_repeat = false;
	bool m_held = false;
	bool m_dragging = false;
	QAtomicInt m_cancel;
	QPointF m_lastPoint;
	QPointF m_dragPrevPoint;
	qreal m_dragTolerance = 0.0;
	bool m_lastConstrain = false;
	bool m_lastCenter = false;
	ToolController::SelectionParams m_pendingParams;
	QImage m_pendingImage;
	QPoint m_pendingPos;
	DragDetector m_dragDetector;
};

}

#endif
