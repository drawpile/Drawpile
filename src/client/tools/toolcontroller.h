/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "utils/strokesmoother.h"
#include "tool.h"

#include <QObject>
#include <QHash>

class QCursor;

namespace canvas { class CanvasModel; }
namespace paintcore { class Brush; }
namespace net { class Client; }
namespace docks { class ToolSettings; }

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
	Q_PROPERTY(canvas::CanvasModel* model READ model WRITE setModel NOTIFY modelChanged)

	Q_OBJECT
public:
	ToolController(net::Client *client, docks::ToolSettings *toolSettings, QObject *parent=nullptr);
	~ToolController();

	void setActiveTool(Tool::Type tool);
	Tool::Type activeTool() const;

	QCursor activeToolCursor() const;

	void setActiveLayer(int id);
	int activeLayer() const { return m_activeLayer; }

	void setActiveAnnotation(int id);
	int activeAnnotation() const { return m_activeAnnotation; }

	void setModel(canvas::CanvasModel *model);
	canvas::CanvasModel *model() const { return m_model; }

	void setSmoothing(int smoothing);
	int smoothing() const { return m_smoothing; }

	inline net::Client *client() const { return m_client; }
	inline docks::ToolSettings *toolSettings() const { return m_toolsettings; }

	paintcore::Brush activeBrush() const;

public slots:
	void startDrawing(const QPointF &point, qreal pressure);
	void continueDrawing(const QPointF &point, qreal pressure, bool shift, bool alt);
	void endDrawing();

signals:
	void activeToolChanged(Tool::Type type);
	void toolCursorChanged(const QCursor &cursor);
	void activeLayerChanged(int layerId);
	void activeAnnotationChanged(int annotationId);
	void modelChanged(canvas::CanvasModel *model);
	void smoothingChanged(int smoothing);

private slots:
	void onAnnotationRowDelete(const QModelIndex&, int first, int last);

private:
	void registerTool(Tool *tool);

	Tool *getTool(Tool::Type type) const;

	QHash<Tool::Type, Tool*> m_toolbox;
	net::Client *m_client;
	docks::ToolSettings *m_toolsettings; // TODO: decouple from UI

	canvas::CanvasModel *m_model;

	Tool *m_activeTool;
	int m_activeLayer;
	int m_activeAnnotation;

	int m_smoothing;
	StrokeSmoother m_smoother;
};

}

#endif // TOOLCONTROLLER_H
