// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FLOODFILL_H
#define TOOLS_FLOODFILL_H
#include "libclient/tools/tool.h"
#include <QAtomicInt>
#include <QCursor>
#include <QImage>
#include <QPoint>

namespace tools {

class FloodFill final : public Tool {
public:
	enum class Source {
		Merged,
		MergedWithoutBackground,
		CurrentLayer,
		FillSourceLayer,
	};

	enum class Area {
		Continuous,
		Similar,
		Selection,
	};

	FloodFill(ToolController &owner);

	void begin(const BeginParams &params) override;
	void motion(const MotionParams &params) override;
	void end() override;
	bool isMultipart() const override;
	void finishMultipart() override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;
	bool usesBrushColor() const override { return true; }
	void setActiveLayer(int layerId) override;
	void setForegroundColor(const QColor &color) override;
	ToolState toolState() const override;

	void setTolerance(qreal tolerance) { m_tolerance = tolerance; }
	void setExpansion(int expansion) { m_expansion = expansion; }
	void setFeatherRadius(int featherRadius)
	{
		m_featherRadius = featherRadius;
	}
	void setSize(int size) { m_size = size; }
	void setOpacity(qreal opacity);
	void setGap(int gap) { m_gap = gap; }
	void setSource(Source source) { m_source = source; }
	void setBlendMode(int blendMode);
	void setArea(Area area) { m_area = area; }

private:
	class Task;
	friend Task;

	void floodFillFinished(Task *task);

	bool havePending() const { return !m_pendingImage.isNull(); }
	void updatePendingPreview();
	void previewPending();
	void flushPending();
	void disposePending();

	void adjustPendingImage(bool adjustOpacity);

	qreal m_tolerance;
	int m_expansion;
	int m_featherRadius;
	int m_size;
	qreal m_opacity;
	int m_gap;
	Source m_source;
	int m_blendMode;
	Area m_area;
	bool m_running;
	QAtomicInt m_cancel;
	QImage m_pendingImage;
	QPoint m_pendingPos;
	Area m_pendingArea;
	QColor m_pendingColor;
};

}

#endif
