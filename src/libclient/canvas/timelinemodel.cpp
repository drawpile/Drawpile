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
#include "libclient/canvas/canvasmodel.h"
#include "libclient/drawdance/layerpropslist.h"
#include "libclient/drawdance/timeline.h"
#include "libclient/drawdance/frame.h"

#include <QBitArray>

namespace canvas {

namespace {
	struct Layer {
		int layerId;
		QString name;
	};

	struct Frame {
		/// the original list of layers in this frame
		QVector<int> layerIds;
	};
}

TimelineModel::TimelineModel(CanvasModel *canvas)
    : QObject(canvas), m_canvas(canvas), m_manualMode(true)
{
}

uint8_t TimelineModel::localUserId() const
{
	return m_canvas->localUserId();
}

void TimelineModel::setLayers(const drawdance::LayerPropsList &lpl)
{
	m_layers.clear();
	m_layerIdsToRows.clear();
	setLayersRecursive(lpl, 0, QString{});
	setLayerIdsToAutoFrame(lpl);
	emit layersChanged();
	if(!m_manualMode) {
		updateAutoFrames();
		emit framesChanged();
	}
}

void TimelineModel::setLayersRecursive(const drawdance::LayerPropsList &lpl, int group, const QString &prefix)
{
	int count = lpl.count();
	for(int i = count - 1; i >= 0; --i) {
		drawdance::LayerProps lp = lpl.at(i);
		drawdance::LayerPropsList children;
		if(lp.isGroup(&children) && !lp.isolated()) {
			setLayersRecursive(children, lp.id(),
				QString{"%1%2 / "}.arg(prefix).arg(lp.title()));
		} else {
			int id = lp.id();
			m_layerIdsToRows[id] = m_layers.count();
			m_layers.append(TimelineLayer{id, group,
				QString{"%1%2"}.arg(prefix).arg(lp.title())});
		}
	}
}

void TimelineModel::setLayerIdsToAutoFrame(const drawdance::LayerPropsList &lpl)
{
	m_layerIdsToAutoFrame.clear();
	int count = lpl.count();
	for(int i = count - 1; i >= 0; --i) {
		setLayerIdsToAutoFrameRecursive(lpl.at(i), i + 1);
	}
}

void TimelineModel::setLayerIdsToAutoFrameRecursive(drawdance::LayerProps lp, int autoFrame)
{
	m_layerIdsToAutoFrame.insert(lp.id(), autoFrame);
	drawdance::LayerPropsList children;
	if(lp.isGroup(&children)) {
		int childCount = children.count();
		for(int i = 0; i < childCount; ++i) {
			setLayerIdsToAutoFrameRecursive(children.at(i), autoFrame);
		}
	}
}

void TimelineModel::setTimeline(const drawdance::Timeline &tl)
{
	int frameCount = tl.frameCount();
	m_frames.resize(frameCount);
	for(int i = 0; i < frameCount; ++i) {
		const drawdance::Frame frame = tl.frameAt(i);
		int layerIdCount = frame.layerIdCount();
		QVector<int> &layerIds = m_frames[i].layerIds;
		layerIds.resize(layerIdCount);
		for (int j = 0; j < layerIdCount; ++j) {
			layerIds[j] = frame.layerIdAt(j);
		}
	}
	emit framesChanged();
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
			m_autoFrames.append(TimelineFrame{{}});
			lastGroup = i->group;
		} else if(i->group != 0) {
			// skip
		} else {
			m_autoFrames.append(TimelineFrame{{i->layerId}});
		}
	}
}

drawdance::Message TimelineModel::makeToggleCommand(int frameCol, int layerRow) const
{
	if(frameCol < 0 || frameCol > m_frames.size() || layerRow < 0 || layerRow >= m_layers.size()) {
		return drawdance::Message::null();
	}

	QVector<uint16_t> layerIds;
	int layerId = m_layers.at(layerRow).layerId;
	if(frameCol == m_frames.size()) {
		layerIds.append(layerId);
	} else {
		for(int frameLayerId : m_frames.at(frameCol).layerIds) {
			layerIds.append(frameLayerId);
		}
		auto i = layerIds.indexOf(layerId);
		if(i == -1) {
			layerIds.append(layerId);
		} else {
			layerIds.remove(i);
		}
	}

	return drawdance::Message::makeSetTimelineFrame(
		m_canvas->localUserId(), frameCol, false, layerIds);
}

drawdance::Message TimelineModel::makeRemoveCommand(int frameCol) const
{
	if(frameCol < 0 || frameCol > m_frames.size()) {
		return drawdance::Message::null();
	}
	return drawdance::Message::makeRemoveTimelineFrame(
		m_canvas->localUserId(), frameCol);
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

int TimelineModel::getAutoFrameForLayerId(int layerId)
{
	return m_layerIdsToAutoFrame.value(layerId, -1);
}

int TimelineModel::nearestLayerTo(int frameIdx, int originalLayer) const
{
	const auto &frames = m_manualMode ? m_frames : m_autoFrames;
	if(frameIdx < 1 || frameIdx > frames.size())
		return 0;

	frameIdx--;

	if(!m_layerIdsToRows.contains(originalLayer))
		return 0;

	const int originalLayerRow = m_layerIdsToRows[originalLayer];

	int nearestRow = -1;
	int nearestDist = 999;

	for(int layerId : frames[frameIdx].layerIds) {
		if(layerId == originalLayer)
			continue;

		const int r = layerRow(layerId);
		const int dist = qAbs(r-originalLayerRow);
		if(dist < nearestDist) {
			nearestRow = r;
			nearestDist = dist;
		}
	}

	return layerRowId(nearestRow);
}

}
