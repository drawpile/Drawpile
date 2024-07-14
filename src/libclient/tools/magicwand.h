// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_MAGICWAND_H
#define LIBCLIENT_TOOLS_MAGICWAND_H
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
	void end(const EndParams &params) override;
	bool isMultipart() const override;
	void finishMultipart() override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;
	ToolState toolState() const override;

	void updateParameters();

private:
	class Task;
	friend Task;

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
	QAtomicInt m_cancel;
	QPointF m_lastPoint;
	bool m_lastConstrain = false;
	bool m_lastCenter = false;
	ToolController::SelectionParams m_pendingParams;
	QImage m_pendingImage;
	QPoint m_pendingPos;
};

}

#endif
