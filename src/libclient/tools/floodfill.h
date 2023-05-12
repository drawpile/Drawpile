// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_FLOODFILL_H
#define TOOLS_FLOODFILL_H

#include "libclient/tools/tool.h"

#include <QCoreApplication>
#include <QAtomicInt>

namespace tools {

class FloodFill final : public Tool
{
	Q_DECLARE_TR_FUNCTIONS(FloodFill)
public:
	FloodFill(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;
	void cancelMultipart() override;

	void setTolerance(qreal tolerance) { m_tolerance = tolerance; }
	void setExpansion(int expansion) { m_expansion = expansion; }
	void setFeatherRadius(int featherRadius) { m_featherRadius = featherRadius; }
	void setSizeLimit(unsigned int limit) { m_sizelimit = qMax(100u, limit); }
	void setSampleMerged(bool sm) { m_sampleMerged = sm; }
	void setBlendMode(int blendMode) { m_blendMode = blendMode; }

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	qreal m_tolerance;
	int m_expansion;
	int m_featherRadius;
	unsigned int m_sizelimit;
	bool m_sampleMerged;
	int m_blendMode;
	bool m_running;
	QAtomicInt m_cancel;
};

}

#endif
