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
	void end() override;
	bool isMultipart() const override;
	void finishMultipart() override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;
	ToolState toolState() const override;

	void updatePendingToolNotice();

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	bool havePending() const { return !m_pendingImage.isNull(); }
	void updateToolNotice();
	void flushPending();
	void disposePending();

	void adjustPendingImage();

	int m_op = -1;
	bool m_running = false;
	QAtomicInt m_cancel;
	QImage m_pendingImage;
	QPoint m_pendingPos;
};

}

#endif
