// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_INSPECTOR_H
#define LIBCLIENT_TOOLS_INSPECTOR_H
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

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end() override;

	void cancelMultipart() override;

	void setShowTiles(bool showTiles) { m_showTiles = showTiles; }

private:
	void inspect(const QPointF &point, bool clobber) const;

	bool m_inspecting = false;
	bool m_showTiles = false;
};

}

#endif
