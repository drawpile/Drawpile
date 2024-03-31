// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FLOODFILL_H
#define TOOLS_FLOODFILL_H
#include "libclient/tools/tool.h"
#include <QAtomicInt>
#include <QCoreApplication>

namespace tools {

class FloodFill final : public Tool {
public:
	FloodFill(ToolController &owner);

	void begin(const canvas::Point &point, bool right, float zoom) override;
	void
	motion(const canvas::Point &point, bool constrain, bool center) override;
	void end() override;
	void cancelMultipart() override;
	bool usesBrushColor() const override { return true; }

	void setTolerance(qreal tolerance) { m_tolerance = tolerance; }
	void setExpansion(int expansion) { m_expansion = expansion; }
	void setFeatherRadius(int featherRadius)
	{
		m_featherRadius = featherRadius;
	}
	void setSize(int size) { m_size = size; }
	void setGap(int gap) { m_gap = gap; }
	void setLayerId(int layerId) { m_layerId = layerId; }
	void setBlendMode(int blendMode) { m_blendMode = blendMode; }
	void setContinuous(bool continuous) { m_continuous = continuous; }

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	qreal m_tolerance;
	int m_expansion;
	int m_featherRadius;
	int m_size;
	int m_gap;
	int m_layerId;
	int m_blendMode;
	bool m_continuous;
	bool m_running;
	QAtomicInt m_cancel;
};

}

#endif
