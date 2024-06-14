// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_TOOLCONTROLLER_H
#define LIBCLIENT_TOOLS_TOOLCONTROLLER_H
#include "libclient/brushes/brush.h"
#include "libclient/canvas/acl.h"
#include "libclient/tools/tool.h"
#include <QObject>
#include <QPainterPath>
#include <QSet>
#include <QThreadPool>

class QCursor;

namespace canvas {
class CanvasModel;
}

namespace drawdance {
class BrushEngine;
}

namespace net {
class Client;
}

namespace tools {

class Tool;
class TransformTool;

/**
 * @brief The ToolController dispatches user input to the currently active tool
 */
class ToolController final : public QObject {
	Q_PROPERTY(QCursor activeToolCursor READ activeToolCursor()
				   NOTIFY toolCursorChanged)
	Q_PROPERTY(uint16_t activeLayer READ activeLayer WRITE setActiveLayer NOTIFY
				   activeLayerChanged)
	Q_PROPERTY(uint16_t activeAnnotation READ activeAnnotation WRITE
				   setActiveAnnotation NOTIFY activeAnnotationChanged)
	Q_PROPERTY(brushes::ActiveBrush activeBrush READ activeBrush WRITE
				   setActiveBrush NOTIFY activeBrushChanged)
	Q_PROPERTY(canvas::CanvasModel *model READ model WRITE setModel NOTIFY
				   modelChanged)

	Q_OBJECT
public:
	enum class SelectionSource { Merged, MergedWithoutBackground, Layer };

	struct SelectionParams {
		bool antiAlias = true;
		int defaultOp = 0;
		qreal tolerance = 0.0;
		int expansion = 0;
		int featherRadius = 0;
		int gap = 0;
		int source = int(SelectionSource::Layer);
		bool continuous = true;
	};

	class Task : public QObject {
	public:
		virtual ~Task() override {}
		virtual void run() = 0;
		virtual void finished() = 0;
	};

	explicit ToolController(net::Client *client, QObject *parent = nullptr);
	~ToolController() override;

	void setActiveTool(Tool::Type tool);
	Tool::Type activeTool() const;

	QCursor activeToolCursor() const;
	bool activeToolAllowColorPick() const;
	bool activeToolAllowToolAdjust() const;
	bool activeToolHandlesRightClick() const;
	bool activeToolIsFractional() const;
	bool activeToolIgnoresSelections() const;

	void setActiveLayer(uint16_t id);
	uint16_t activeLayer() const { return m_activeLayer; }

	void setActiveAnnotation(uint16_t id);
	uint16_t activeAnnotation() const { return m_activeAnnotation; }

	void setActiveBrush(const brushes::ActiveBrush &b);
	const brushes::ActiveBrush &activeBrush() const { return m_activebrush; }

	bool isDrawing() const { return m_drawing; }

	void setInterpolateInputs(bool interpolateInputs);

	void setStabilizerUseBrushSampleCount(bool stabilizerUseBrushSampleCount);
	bool stabilizerUseBrushSampleCount()
	{
		return m_stabilizerUseBrushSampleCount;
	}

	void setStabilizationMode(brushes::StabilizationMode stabilizationMode);
	int stabilizationMode() { return m_stabilizationMode; }

	void setStabilizerSampleCount(int stabilizerSampleCount);
	int stabilizerSampleCount() { return m_stabilizerSampleCount; }

	void setSmoothing(int smoothing);
	int smoothing() { return m_smoothing; }

	void setFinishStrokes(bool finishStrokes);
	bool finishStrokes() { return m_finishStrokes; }

	void setModel(canvas::CanvasModel *model);
	canvas::CanvasModel *model() const { return m_model; }

	void setGlobalSmoothing(int smoothing);
	int globalSmoothing() const { return m_globalSmoothing; }

	void setMouseSmoothing(bool mouseSmoothing);

	void setTransformParams(bool accurate, int interpolation);
	int transformInterpolation() const { return m_transformInterpolation; }

	const SelectionParams &selectionParams() const { return m_selectionParams; }
	void setSelectionParams(const SelectionParams &selectionParams)
	{
		m_selectionParams = selectionParams;
	}

	net::Client *client() const { return m_client; }

	Tool *getTool(Tool::Type type);
	TransformTool *transformTool();

	//! Is there a multipart drawing operation in progress?
	bool isMultipartDrawing() const;

	/**
	 * Apply an offset to the position of the active tool
	 *
	 * This is used to correct the tool position when the canvas is
	 * resized while the local user is still drawing.
	 */
	void offsetActiveTool(int xOffset, int yOffset);

	/**
	 * Set the active brush in the Drawdance brush engine.
	 *
	 * The freehand parameter can be used to turn off stabilizer
	 * interference when drawing previews, shapes, lines and curves.
	 */
	void setBrushEngineBrush(drawdance::BrushEngine &be, bool freehand);

	/**
	 * Runs the given task in the background. Takes over the task using
	 * setParent(), calls run() on it to execute it on a background thread and
	 * finished() on the main thread when it's done, then calls deleteLater().
	 */
	void executeAsync(Task *task);

public slots:
	//! Start a new stroke
	void startDrawing(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool right, qreal angle, qreal zoom,
		bool mirror, bool flip, bool constrain, bool center,
		const QPointF &viewPos, int deviceType, bool eraserOverride);

	//! Continue a stroke
	void continueDrawing(
		long long timeMsec, const QPointF &point, qreal pressure, qreal xtilt,
		qreal ytilt, qreal rotation, bool constrain, bool center,
		const QPointF &viewPos);

	//! Stylus hover (not yet drawing)
	void hoverDrawing(
		const QPointF &point, qreal angle, qreal zoom, bool mirror, bool flip);

	//! End a stroke
	void endDrawing();

	/**
	 * @brief Undo the latest part of a multipart drawing
	 *
	 * Multipart drawings are not committed until finishMultipartDrawing is
	 * called, so undoing is a local per-tool operation.
	 *
	 * @return false if there was nothing to undo
	 */
	bool undoMultipartDrawing();
	bool redoMultipartDrawing();

	//! Commit the current multipart drawing (if any)
	void finishMultipartDrawing();

	//! Cancel the current multipart drawing (if any)
	void cancelMultipartDrawing();

	void deselectDeletedAnnotation(int annotationId);

signals:
	void toolCapabilitiesChanged(
		bool allowColorPick, bool allowToolAdjust, bool allowRightClick,
		bool fractionalTool, bool ignoresSelections);
	void toolCursorChanged(const QCursor &cursor);
	void activeLayerChanged(int layerId);
	void activeAnnotationChanged(uint16_t annotationId);
	void activeBrushChanged(const brushes::ActiveBrush &);
	void transformToolStateChanged(int mode, int handle, bool dragging);
	void modelChanged(canvas::CanvasModel *model);
	void globalSmoothingChanged(int smoothing);
	void stabilizerUseBrushSampleCountChanged(bool useBrushSampleCount);
	void actionCancelled();

	void colorUsed(const QColor &color);
	void panRequested(int x, int y);
	void zoomRequested(const QRect &rect, int steps);
	void pathPreviewRequested(const QPainterPath &path);
	void transformRequested();
	void toolSwitchRequested(tools::Tool::Type tool);
	void showMessageRequested(const QString &message);

	void busyStateChanged(bool busy);
	void asyncExecutionFinished(Task *task);

private slots:
	void onFeatureAccessChange(DP_Feature feature, bool canUse);
	void updateTransformPreview();
	void setTransformCutPreview(
		const QSet<int> &layerIds, const QRect &maskBounds, const QImage &mask);
	void clearTransformCutPreview();
	void clearTransformPreviews();
	void notifyAsyncExecutionFinished(Task *task);

private:
	void registerTool(Tool *tool);
	void updateSmoothing();

	Tool *m_toolbox[Tool::_LASTTOOL];
	net::Client *m_client;

	canvas::CanvasModel *m_model;

	brushes::ActiveBrush m_activebrush;
	Tool *m_activeTool;
	uint16_t m_activeLayer;
	uint16_t m_activeAnnotation;
	bool m_drawing;
	bool m_applyGlobalSmoothing;
	bool m_mouseSmoothing;

	int m_globalSmoothing;
	bool m_interpolateInputs;
	brushes::StabilizationMode m_stabilizationMode;
	int m_stabilizerSampleCount;
	int m_smoothing;
	int m_effectiveSmoothing;
	bool m_finishStrokes;
	bool m_stabilizerUseBrushSampleCount;

	bool m_transformPreviewAccurate;
	int m_transformInterpolation;
	int m_transformPreviewIdsUsed;
	SelectionParams m_selectionParams;

	QThreadPool m_threadPool;
	QAtomicInt m_taskCount;
};

}

#endif
