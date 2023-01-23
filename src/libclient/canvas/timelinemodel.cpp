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

#include "libclient/canvas/timelinemodel.h"
#include "libclient/canvas/layerlist.h"
#include "libclient/net/envelopebuilder.h"
#include "rustpile/rustpile.h"

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
    : QObject(parent), m_manualMode(true)
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
		    l.left < prefixUntil ? prefixUntil : 0,
		    l.left < prefixUntil ? QStringLiteral("%1 / %2").arg(prefix, l.title) : l.title
	    };
		m_layerIdsToRows[l.id] = m_layers.size() - 1;
	}

	emit layersChanged();
	if(!m_manualMode) {
		updateAutoFrames();
		emit framesChanged();
	}
}

void timelineUpdateFrames(void *ctx, const rustpile::Frame *frames, uintptr_t count)
{
	TimelineModel *model = reinterpret_cast<TimelineModel*>(ctx);
	model->m_frames.resize(count);
	memcpy(model->m_frames.data(), frames, sizeof(rustpile::Frame) * count);
	emit model->framesChanged();
}

void TimelineModel::updateAutoFrames()
{
	m_autoFrames.clear();
	int lastGroup = 0;
	// This odd ruleset reflects the way the paint engine handles non-isolated
	// groups in auto-timeline mode.
	// TODO: treat non-isolated groups the same as isolated groups in auto-mode.
	// This could be represented in the UI by showing all the layers in the group as
	// selected.
	for(auto i=m_layers.rbegin();i!=m_layers.rend();++i) {
		if(i->group != 0 && i->group != lastGroup) {
			m_autoFrames.append(TimelineFrame{{0}});
			lastGroup = i->group;
		} else if(i->group != 0) {
			// skip
		} else {
			m_autoFrames.append(TimelineFrame{{i->id}});
		}
	}
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

void TimelineModel::makeRemoveCommand(net::EnvelopeBuilder &eb, int frameCol) const
{
	if(frameCol < 0 || frameCol > m_frames.size())
		return;
	rustpile::write_removetimelineframe(eb, 0, frameCol);
}

void TimelineModel::setManualMode(bool manual)
{
	if(m_manualMode != manual) {
		m_manualMode = manual;
		if(!manual)
			updateAutoFrames();
		emit framesChanged();
	}
}

rustpile::LayerID TimelineModel::nearestLayerTo(int frameIdx, rustpile::LayerID originalLayer) const
{
	const auto &frames = m_manualMode ? m_frames : m_autoFrames;
	if(frameIdx < 1 || frameIdx > frames.size())
		return 0;

	frameIdx--;

	if(!m_layerIdsToRows.contains(originalLayer))
		return 0;

	const int originalLayerRow = m_layerIdsToRows[originalLayer];
	const rustpile::Frame &frame = frames[frameIdx].frame;

	int nearestRow = -1;
	int nearestDist = 999;

	for(unsigned int i=0;i<sizeof(rustpile::Frame)/sizeof(rustpile::LayerID);++i) {
		if(frame[i] == 0)
			break;
		if(frame[i] == originalLayer)
			continue;

		const int r = layerRow(frame[i]);
		const int dist = qAbs(r-originalLayerRow);
		if(dist < nearestDist) {
			nearestRow = r;
			nearestDist = dist;
		}
	}

	return layerRowId(nearestRow);
}

}
