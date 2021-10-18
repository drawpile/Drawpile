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

#include "../rustpile/rustpile.h"
#include "layerlist.h"

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
		QString name;
	};

	explicit TimelineModel(QObject *parent=nullptr);

	const QVector<TimelineFrame> &frames() const { return m_frames; }
	const QVector<TimelineLayer> &layers() const { return m_layers; }
	int layerRow(rustpile::LayerID id) const { return m_layerIdsToRows[id]; }

	void makeToggleCommand(net::EnvelopeBuilder &eb, int frameCol, int layerRow) const;

public slots:
	void setLayers(const QVector<LayerListItem> &layers);

signals:
	void layersChanged();
	void framesChanged();

private:
	QVector<TimelineFrame> m_frames;
	QVector<TimelineLayer> m_layers;
	QHash<rustpile::LayerID, int> m_layerIdsToRows;
};

void timelineUpdateFrames(void *ctx, const rustpile::Frame *frames, uintptr_t count);

}

#endif
