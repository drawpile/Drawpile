// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_MAGICWAND_H
#define LIBCLIENT_TOOLS_MAGICWAND_H
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"
#include "libshared/net/message.h"

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

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	bool havePending() const { return !m_pending.isEmpty(); }
	void flushPending();
	void disposePending();

	int m_op = -1;
	bool m_running = false;
	QAtomicInt m_cancel;
	net::MessageList m_pending;
};

}

#endif
