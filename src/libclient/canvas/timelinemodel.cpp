/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#include "timelinemodel.h"
#include "layerlist.h"
#include "../net/envelopebuilder.h"
#include "../../rustpile/rustpile.h"

#include <QBitArray>

namespace canvas {

namespace {
	struct Layer {
		rustpile::LayerID id;
		QString name;
	};

	static const int MAX_LAYERS_PER_FRAME = sizeof(rustpile::Frame) / sizeof(rustpile::LayerID);

	struct Frame {
		/// the original list of layers in this frame
		rustpile::Frame frame = {0};

		/// lookup table generated from the above
		QBitArray rows;

		/// Regenerate the lookup table
		void updateRows(const QVector<Layer> &layers)
		{
			rows.clear();
			rows.resize(layers.size());
			for(int i=0;i<MAX_LAYERS_PER_FRAME;++i) {
				if(frame[i] == 0)
					break;
				// TODO needs benchmarking? Number of layers is generally low enough
				// that a linear search may still be faster than a hashmap?
				for(int j=0;j<layers.size();++i) {
					if(layers.at(j).id == frame[i]) {
						rows.setBit(j);
						break;
					}
				}
			}
		}
	};
}

TimelineModel::TimelineModel(QObject *parent)
    : QObject(parent)
{
}

void TimelineModel::setLayers(const QVector<LayerListItem> &layers)
{
	m_layers.clear();
	m_layerIdsToRows.clear();

	int skipUntil=-1;
	int prefixUntil=-1;
	QString prefix;

	for(const auto &l : layers) {
		if(l.left < skipUntil)
			continue;

		if(l.group) {
			if(l.isolated) {
				// We don't include the content of isolated groups in the timeline,
				// as we want to treat them as individual layers
				skipUntil = l.right;
			} else {
				// But we do want to see the content of non-isolated groups.
				// They can be used to group together related frames
				prefix = l.title;
				prefixUntil = l.right;
				continue;
			}
		}

		m_layers << TimelineLayer {
		    l.id,
		    l.left < prefixUntil ? QStringLiteral("%1 / %2").arg(prefix, l.title) : l.title
	    };
		m_layerIdsToRows[l.id] = m_layers.size() - 1;
	}

	emit layersChanged();
}

void timelineUpdateFrames(void *ctx, const rustpile::Frame *frames, uintptr_t count)
{
	TimelineModel *model = reinterpret_cast<TimelineModel*>(ctx);
	model->m_frames.resize(count);
	memcpy(model->m_frames.data(), frames, sizeof(rustpile::Frame) * count);
	emit model->framesChanged();
}

void TimelineModel::makeToggleCommand(net::EnvelopeBuilder &eb, int frameCol, int layerRow) const
{
	if(frameCol < 0 || frameCol > m_frames.size())
		return;
	if(layerRow < 0 || layerRow >= m_layers.size())
		return;

	TimelineFrame frame;

	const rustpile::LayerID layerId = m_layers.at(layerRow).id;

	if(frameCol == m_frames.size()) {
		frame = { layerId, 0 };

	} else {
		frame = m_frames.at(frameCol);

		for(int i=0;i<MAX_LAYERS_PER_FRAME;++i) {
			if(frame.frame[i] == layerId) {
				for(int j=i;j<MAX_LAYERS_PER_FRAME-1;++j) {
					frame.frame[j] = frame.frame[j+1];
				}
				frame.frame[MAX_LAYERS_PER_FRAME-1] = 0;
				break;
			} else if(frame.frame[i] == 0) {
				frame.frame[i] = layerId;
				break;
			}
		}
	}

	rustpile::write_settimelineframe(eb, 0, frameCol, false, reinterpret_cast<const uint16_t*>(&frame.frame), MAX_LAYERS_PER_FRAME);
}

}
