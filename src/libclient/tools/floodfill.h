// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef TOOLS_FLOODFILL_H
#define TOOLS_FLOODFILL_H
#include "libclient/tools/clickdetector.h"
#include "libclient/tools/tool.h"
#include <QAtomicInt>
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
	void flushPreviewedActions() override;
	bool usesBrushColor() const override { return true; }
	void setActiveLayer(int layerId) override;
	void setLayerAlphaLock(bool alphaLock) override;
	void setForegroundColor(const QColor &color) override;

	void setParameters(
		int tolerance, int expansion, int kernel, int featherRadius, int size,
		qreal opacity, int gap, Source source, int blendMode, Area area,
		bool editableFills, bool confirmFills);

private:
	class Task;
	friend Task;

	int lastActiveLayerId() const;

	void stopDragging();
	void fillAt(const QPointF &point, int activeLayerId, bool editable);
	void repeatFill();
	void floodFillFinished(Task *task);

	bool havePending() const { return !m_pendingImage.isNull(); }
	void updatePendingPreview();
	void previewPending();
	void flushPending();
	void disposePending();

	void adjustPendingImage(bool adjustColor, bool adjustOpacity);

	void updateCursor();

	void emitFloodFillStateChanged();

	int getEffectiveBlendModeForLayer(
		int layerId, bool canUseOriginal = true) const;

	static bool blendModeHandlesColor(int blendMode);

	int m_tolerance = 0;
	int m_expansion = 0;
	int m_kernel;
	int m_featherRadius = 0;
	int m_size = 500;
	qreal m_opacity = 1.0;
	int m_gap = 0;
	Source m_source = Source::CurrentLayer;
	int m_blendMode;
	Area m_area = Area::Continuous;
	bool m_editableFills = false;
	bool m_confirmFills = false;
	bool m_running = false;
	bool m_repeat = false;
	bool m_pendingEditable = false;
	bool m_dragging = false;
	QAtomicInt m_cancel = false;
	QPointF m_lastPoint;
	QPointF m_dragPrevPoint;
	qreal m_dragTolerance = 0.0;
	int m_lastActiveLayerId = 0;
	QImage m_pendingImage;
	QPoint m_pendingPos;
	Area m_pendingArea;
	QColor m_pendingColor;
	int m_originalLayerId = 0;
	int m_originalBlendMode;
	qreal m_originalOpacity;
	DragDetector m_dragDetector;
};

}

#endif
