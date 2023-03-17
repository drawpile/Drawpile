/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2022 Calle Laakkonen

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
#ifndef DP_TIMELINE_MODEL_H
#define DP_TIMELINE_MODEL_H

#include "libclient/canvas/layerlist.h"
#include "libclient/drawdance/message.h"

#include <QVector>
#include <QHash>
#include <QObject>

namespace drawdance {
	class LayerProps;
	class LayerPropsList;
	class Timeline;
}

namespace canvas {

class CanvasModel;

class TimelineModel final : public QObject {
	Q_OBJECT
public:
	struct TimelineFrame {
		QVector<int> layerIds;
	};

	struct TimelineLayer {
		int layerId;
		int group;
		QString name;
	};

	explicit TimelineModel(CanvasModel *canvas);

	uint8_t localUserId() const;
	const QVector<TimelineFrame> &frames() const { return m_manualMode ? m_frames : m_autoFrames; }
	const QVector<TimelineLayer> &layers() const { return m_layers; }

	int layerRow(int layerId) const { return m_layerIdsToRows[layerId]; }

	int layerRowId(int row) const {
		if(row >= 0 && row < m_layers.size()) {
			return m_layers[row].layerId;
		} else {
			return 0;
		}
	}

	int nearestLayerTo(int frame, int nearest) const;

	// These may return null messages!
	drawdance::Message makeToggleCommand(int frameCol, int layerRow) const;
	drawdance::Message makeRemoveCommand(int frameCol) const;

	void setManualMode(bool manual);
	bool isManualMode() const { return m_manualMode; }

	int getAutoFrameForLayerId(int layerId);

public slots:
	void setLayers(const drawdance::LayerPropsList &lpl);
	void setTimeline(const drawdance::Timeline &tl);

signals:
	void layersChanged();
	void framesChanged();

private:
	void setLayersRecursive(const drawdance::LayerPropsList &lpl, int group, const QString &prefix);
	void setLayerIdsToAutoFrame(const drawdance::LayerPropsList &lpl);
	void setLayerIdsToAutoFrameRecursive(drawdance::LayerProps lp, int autoFrame);
	void updateAutoFrames();

	CanvasModel *m_canvas;
	QVector<TimelineFrame> m_frames;
	QVector<TimelineFrame> m_autoFrames;
	QVector<TimelineLayer> m_layers;
	QHash<int, int> m_layerIdsToRows;
	QHash<int, int> m_layerIdsToAutoFrame;
	bool m_manualMode;
};

}

#endif
