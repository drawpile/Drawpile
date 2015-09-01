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

#ifndef CANVASMODEL_H
#define CANVASMODEL_H

#include <QObject>

#include "../shared/net/message.h"

// note: we must include these so the datatypes get registered properly for use in QML
#include "usercursormodel.h"
#include "lasertrailmodel.h"
#include "selection.h"
#include "core/layerstack.h"

namespace net {
	class Client;
	class LayerListModel;
}

namespace canvas {

class StateTracker;

class CanvasModel : public QObject
{
	Q_PROPERTY(paintcore::LayerStack* layerStack READ layerStack CONSTANT)
	Q_PROPERTY(UserCursorModel* userCursors READ userCursors CONSTANT)
	Q_PROPERTY(LaserTrailModel* laserTrails READ laserTrails CONSTANT)
	Q_PROPERTY(StateTracker* stateTracker READ stateTracker CONSTANT)
	Q_PROPERTY(Selection* selection READ selection WRITE setSelection NOTIFY selectionChanged)

	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)

	Q_OBJECT

public:
	explicit CanvasModel(net::Client *client, QObject *parent = 0);

	paintcore::LayerStack *layerStack() const { return m_layerstack; }
	StateTracker *stateTracker() const { return m_statetracker; }
	UserCursorModel *userCursors() const { return m_usercursors; }
	LaserTrailModel *laserTrails() const { return m_lasers; }

	QString title() const { return m_title; }
	void setTitle(const QString &title) { if(m_title!=title) { m_title = title; emit titleChanged(title); } }

	Selection *selection() const { return m_selection; }
	void setSelection(Selection *selection);

	bool needsOpenRaster() const;
	QImage toImage() const;
	bool save(const QString &filename) const;

	QList<protocol::MessagePtr> generateSnapshot(bool forceNew) const;

	int getAvailableAnnotationId() const;

	QImage selectionToImage(int layerId) const;
	void pasteFromImage(const QImage &image, const QPoint &defaultPoint, bool forceDefault);

public slots:
	void handleDrawingCommand(protocol::MessagePtr cmd);
	void handleLocalCommand(protocol::MessagePtr cmd);

	void resetCanvas();

	void pickColor(int x, int y, int layer, int diameter=0);

	void setLayerViewMode(int mode);
	void updateLayerViewOptions();

signals:
	void layerAutoselectRequest(int id);
	void canvasModified();
	void selectionChanged(Selection *selection);
	void selectionRemoved();

	void titleChanged(QString title);
	void imageSizeChanged();

	void colorPicked(const QColor &color);

private slots:
	void onCanvasResize(int xoffset, int yoffset, const QSize &oldsize);

private:
	paintcore::LayerStack *m_layerstack;
	net::LayerListModel *m_layerlist;
	StateTracker *m_statetracker;
	UserCursorModel *m_usercursors;
	LaserTrailModel *m_lasers;
	Selection *m_selection;

	QString m_title;
};

}

#endif // CANVASSTATE_H
