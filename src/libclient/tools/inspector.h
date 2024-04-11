// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_INSPECTOR_H
#define TOOLS_INSPECTOR_H
#include "libclient/tools/tool.h"

namespace tools {

/**
 * \brief Canvas inspector
 *
 * A moderation tool: show who last edited the selected tile
 */
class Inspector final : public Tool {
public:
	Inspector(ToolController &owner);

	void begin(
		const canvas::Point &point, bool right, float zoom,
		const QPointF &viewPos) override;

	void motion(
		const canvas::Point &point, bool constrain, bool center,
		const QPointF &viewPos) override;

	void end() override;

	void cancelMultipart() override;

	void setShowTiles(bool showTiles) { m_showTiles = showTiles; }

private:
	bool m_inspecting = false;
	bool m_showTiles = false;
};

}

#endif
