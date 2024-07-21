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
	void end(const EndParams &params) override;
	bool isMultipart() const override;
	void finishMultipart() override;
	void undoMultipart() override;
	void cancelMultipart() override;
	void dispose() override;
	bool usesBrushColor() const override { return true; }
	void setActiveLayer(int layerId) override;
	void setForegroundColor(const QColor &color) override;
	ToolState toolState() const override;

	void setParameters(
		qreal tolerance, int expansion, int featherRadius, int size,
		qreal opacity, int gap, Source source, int blendMode, Area area);

private:
	class Task;
	friend Task;

	int lastActiveLayerId() const;

	void fillAt(const QPointF &point, int activeLayerId);
	void repeatFill();
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
	bool m_repeat;
	QAtomicInt m_cancel;
	QPointF m_lastPoint;
	int m_lastActiveLayerId;
	QImage m_pendingImage;
	QPoint m_pendingPos;
	Area m_pendingArea;
	QColor m_pendingColor;
};

}

#endif
