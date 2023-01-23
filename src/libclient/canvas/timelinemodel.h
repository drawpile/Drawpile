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

#include "rustpile/rustpile.h"
#include "libclient/canvas/layerlist.h"

#include <QVector>
#include <QHash>
#include <QObject>

namespace net {
	class EnvelopeBuilder;
}

namespace canvas {

class TimelineModel : public QObject {
	Q_OBJECT
	friend void timelineUpdateFrames(void *ctx, const rustpile::Frame *frames, uintptr_t count);
public:
	struct TimelineFrame {
		rustpile::Frame frame;
	};

	struct TimelineLayer {
		rustpile::LayerID id;
		int group;
		QString name;
	};

	explicit TimelineModel(QObject *parent=nullptr);

	const QVector<TimelineFrame> &frames() const { return m_manualMode ? m_frames : m_autoFrames; }
	const QVector<TimelineLayer> &layers() const { return m_layers; }
	int layerRow(rustpile::LayerID id) const { return m_layerIdsToRows[id]; }
	rustpile::LayerID layerRowId(int row) const {
		if(row >= 0 && row < m_layers.size()) {
			return m_layers[row].id;
		} else {
			return 0;
		}
	}
        rustpile::LayerID nearestLayerTo(int frame, rustpile::LayerID nearet) const;

	void makeToggleCommand(net::EnvelopeBuilder &eb, int frameCol, int layerRow) const;
	void makeRemoveCommand(net::EnvelopeBuilder &eb, int frameCol) const;

	void setManualMode(bool manual);
	bool isManualMode() const { return m_manualMode; }

public slots:
	void setLayers(const QVector<LayerListItem> &layers);

signals:
	void layersChanged();
	void framesChanged();

private:
	void updateAutoFrames();
	QVector<TimelineFrame> m_frames;
	QVector<TimelineFrame> m_autoFrames;
	QVector<TimelineLayer> m_layers;
	QHash<rustpile::LayerID, int> m_layerIdsToRows;
	bool m_manualMode;
};

void timelineUpdateFrames(void *ctx, const rustpile::Frame *frames, uintptr_t count);

}

#endif
