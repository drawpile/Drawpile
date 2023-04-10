// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLCONTROLLER_H
#define TOOLCONTROLLER_H

#include "libclient/tools/strokesmoother.h"
#include "libclient/tools/tool.h"
#include "libclient/brushes/brush.h"
#include "libclient/canvas/acl.h"

#include <QObject>

class QCursor;

namespace canvas {
	class CanvasModel;
	class Selection;
}
namespace drawdance { class BrushEngine; }
namespace net { class Client; }

namespace tools {

class Tool;

/**
 * @brief The ToolController dispatches user input to the currently active tool
 */
class ToolController final : public QObject
{
	Q_PROPERTY(QCursor activeToolCursor READ activeToolCursor() NOTIFY toolCursorChanged)
	Q_PROPERTY(int smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
	Q_PROPERTY(uint16_t activeLayer READ activeLayer WRITE setActiveLayer NOTIFY activeLayerChanged)
	Q_PROPERTY(uint16_t activeAnnotation READ activeAnnotation WRITE setActiveAnnotation NOTIFY activeAnnotationChanged)
	Q_PROPERTY(brushes::ActiveBrush activeBrush READ activeBrush WRITE setActiveBrush NOTIFY activeBrushChanged)
	Q_PROPERTY(canvas::CanvasModel* model READ model WRITE setModel NOTIFY modelChanged)

	Q_OBJECT
public:
	explicit ToolController(net::Client *client, QObject *parent=nullptr);
	~ToolController() override;

	void setActiveTool(Tool::Type tool);
	Tool::Type activeTool() const;

	QCursor activeToolCursor() const;
	bool activeToolAllowColorPick() const;
	bool activeToolAllowToolAdjust() const;
	bool activeToolHandlesRightClick() const;

	void setActiveLayer(uint16_t id);
	uint16_t activeLayer() const { return m_activeLayer; }

	void setActiveAnnotation(uint16_t id);
	uint16_t activeAnnotation() const { return m_activeAnnotation; }

	void setActiveBrush(const brushes::ActiveBrush &b);
	const brushes::ActiveBrush &activeBrush() const { return m_activebrush; }

	void setModel(canvas::CanvasModel *model);
	canvas::CanvasModel *model() const { return m_model; }

	void setSmoothing(int smoothing);
	int smoothing() const { return m_smoothing; }

	void setSelectInterpolation(int selectInterpolation);
	int selectInterpolation() const { return m_selectInterpolation; }

	// TODO this is used just for sending the commands. Replace with a signal?
	inline net::Client *client() const { return m_client; }

	Tool *getTool(Tool::Type type);

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
	void setBrushEngineBrush(drawdance::BrushEngine &be, bool freehand = true);

public slots:
	//! Start a new stroke
	void startDrawing(const QPointF &point, qreal pressure, bool right, float zoom);

	//! Continue a stroke
	void continueDrawing(const QPointF &point, qreal pressure, bool shift, bool alt);

	//! Stylus hover (not yet drawing)
	void hoverDrawing(const QPointF &point);

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

	//! Commit the current multipart drawing (if any)
	void finishMultipartDrawing();

	//! Cancel the current multipart drawing (if any)
	void cancelMultipartDrawing();

signals:
	void toolCapabilitiesChanged(
		bool allowColorPick, bool allowToolAdjust, bool allowRightClick);
	void toolCursorChanged(const QCursor &cursor);
	void activeLayerChanged(int layerId);
	void activeAnnotationChanged(uint16_t annotationId);
	void activeBrushChanged(const brushes::ActiveBrush&);
	void modelChanged(canvas::CanvasModel *model);
	void smoothingChanged(int smoothing);

	void colorUsed(const QColor &color);
	void zoomRequested(const QRect &rect, int steps);

	void toolTip(const QString &message);

private slots:
	void onFeatureAccessChange(DP_Feature feature, bool canUse);
	void onSelectionChange(canvas::Selection *sel);
	void updateSelectionPreview();

private:
	void registerTool(Tool *tool);

	Tool *m_toolbox[Tool::_LASTTOOL];
	net::Client *m_client;

	canvas::CanvasModel *m_model;

	brushes::ActiveBrush m_activebrush;
	Tool *m_activeTool;
	uint16_t m_activeLayer;
	uint16_t m_activeAnnotation;
	bool m_prevShift, m_prevAlt;

	int m_smoothing;
	StrokeSmoother m_smoother;

	int m_selectInterpolation;
};

}

#endif // TOOLCONTROLLER_H
