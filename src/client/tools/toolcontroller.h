/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2016 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef TOOLCONTROLLER_H
#define TOOLCONTROLLER_H

#include "strokesmoother.h"
#include "tool.h"
#include "brushes/brush.h"
#include "canvas/features.h"

#include <QObject>

class QCursor;

namespace canvas { class CanvasModel; }
namespace net { class Client; }

namespace tools {

class Tool;

/**
 * @brief The ToolController dispatches user input to the currently active tool
 */
class ToolController : public QObject
{
	Q_PROPERTY(QCursor activeToolCursor READ activeToolCursor() NOTIFY toolCursorChanged)
	Q_PROPERTY(int smoothing READ smoothing WRITE setSmoothing NOTIFY smoothingChanged)
	Q_PROPERTY(int activeLayer READ activeLayer WRITE setActiveLayer NOTIFY activeLayerChanged)
	Q_PROPERTY(int activeAnnotation READ activeAnnotation WRITE setActiveAnnotation NOTIFY activeAnnotationChanged)
	Q_PROPERTY(brushes::ClassicBrush activeBrush READ activeBrush WRITE setActiveBrush NOTIFY activeBrushChanged)
	Q_PROPERTY(canvas::CanvasModel* model READ model WRITE setModel NOTIFY modelChanged)

	Q_OBJECT
public:
	explicit ToolController(net::Client *client, QObject *parent=nullptr);
	~ToolController();

	void setActiveTool(Tool::Type tool);
	Tool::Type activeTool() const;

	QCursor activeToolCursor() const;

	void setActiveLayer(int id);
	int activeLayer() const { return m_activeLayer; }

	void setActiveAnnotation(int id);
	int activeAnnotation() const { return m_activeAnnotation; }

	void setActiveBrush(const brushes::ClassicBrush &b);
	const brushes::ClassicBrush &activeBrush() const { return m_activebrush; }

	void setModel(canvas::CanvasModel *model);
	canvas::CanvasModel *model() const { return m_model; }

	void setSmoothing(int smoothing);
	int smoothing() const { return m_smoothing; }

	// TODO this is used just for sending the commands. Replace with a signal?
	inline net::Client *client() const { return m_client; }

	Tool *getTool(Tool::Type type);

	//! Is there a multipart drawing operation in progress?
	bool isMultipartDrawing() const;

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
	void activeToolChanged(Tool::Type type);
	void toolCursorChanged(const QCursor &cursor);
	void activeLayerChanged(int layerId);
	void activeAnnotationChanged(int annotationId);
	void activeBrushChanged(const brushes::ClassicBrush&);
	void modelChanged(canvas::CanvasModel *model);
	void smoothingChanged(int smoothing);

	void colorUsed(const QColor &color);

private slots:
	void onAnnotationRowDelete(const QModelIndex&, int first, int last);
	void onFeatureAccessChange(canvas::Feature feature, bool canUse);

private:
	void registerTool(Tool *tool);

	Tool *m_toolbox[Tool::_LASTTOOL];
	net::Client *m_client;

	canvas::CanvasModel *m_model;

	brushes::ClassicBrush m_activebrush;
	Tool *m_activeTool;
	int m_activeLayer;
	int m_activeAnnotation;
	bool m_prevShift, m_prevAlt;

	int m_smoothing;
	StrokeSmoother m_smoother;
};

}

#endif // TOOLCONTROLLER_H
