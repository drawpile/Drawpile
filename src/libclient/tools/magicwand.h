// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_MAGICWAND_H
#define LIBCLIENT_TOOLS_MAGICWAND_H
#include "libclient/tools/tool.h"
#include "libclient/tools/toolcontroller.h"

namespace tools {

class MagicWandTool final : public Tool {
public:
	MagicWandTool(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end() override;
	bool isMultipart() const override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	int m_op = -1;
	bool m_running;
	QAtomicInt m_cancel;
};

}

#endif
