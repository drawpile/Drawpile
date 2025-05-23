// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
#include <dpmsg/ids.h>
}
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/documentmetadata.h"
#include "libclient/canvas/timelinemodel.h"
#include "libclient/drawdance/timeline.h"
#include <QRegularExpression>
#include <QSet>

namespace canvas {

QString
TimelineKeyFrame::makeTitleWithColor(const QString &title, const QColor &color)
{
	if(color.isValid() && color.alpha() > 0) {
		return QStringLiteral("%1!%2").arg(color.name(QColor::HexRgb), title);
	} else {
		return title;
	}
}

const TimelineKeyFrame *
TimelineTrack::searchKeyFrameByFrameIndex(int frameIndex) const
{
	QHash<int, int>::const_iterator it =
		keyFrameIndexesByFrameIndex.constFind(frameIndex);
	return it == keyFrameIndexesByFrameIndex.constEnd()
			   ? nullptr
			   : &keyFrames[it.value()];
}

TimelineModel::TimelineModel(CanvasModel *canvas)
	: QAbstractItemModel(canvas)
{
	DocumentMetadata *metadata = canvas->metadata();
	connect(
		metadata, &DocumentMetadata::frameCountChanged, this,
		&TimelineModel::setFrameCount);
	m_frameCount = metadata->frameCount();
}

const TimelineTrack *TimelineModel::getTrackById(int trackId) const
{
	for(const TimelineTrack &track : m_tracks) {
		if(track.id == trackId) {
			return &track;
		}
	}
	return nullptr;
}

int TimelineModel::getAvailableTrackId() const
{
	QSet<int> takenIds;
	takenIds.insert(0);
	for(const TimelineTrack &track : m_tracks) {
		takenIds.insert(track.id);
	}

	unsigned int localUserId = m_aclState ? m_aclState->localUserId() : 0u;
	int trackId = searchAvailableTrackId(takenIds, localUserId);
	if(trackId != 0) {
		return trackId;
	}

	if(m_aclState->amOperator()) {
		trackId = searchAvailableTrackId(takenIds, 0);
		if(trackId != 0) {
			return trackId;
		}

		for(unsigned int i = 255; i > 0; --i) {
			if(i != localUserId) {
				trackId = searchAvailableTrackId(takenIds, i);
				if(trackId != 0) {
					return trackId;
				}
			}
		}
	}

	return 0;
}

QString TimelineModel::getAvailableTrackName(QString basename) const
{
	static QRegularExpression suffixNumRe{QStringLiteral("\\s*([0-9])+\\z")};
	QRegularExpressionMatch suffixNumMatch = suffixNumRe.match(basename);
	if(suffixNumMatch.hasMatch()) {
		basename = basename.mid(0, suffixNumMatch.capturedStart());
	}

	QRegularExpression re{QStringLiteral("\\A%1\\s*([0-9]+)\\z").arg(basename)};
	int maxFound = 0;
	for(const TimelineTrack &track : m_tracks) {
		QRegularExpressionMatch match = re.match(track.title);
		if(match.hasMatch()) {
			maxFound = qMax(maxFound, match.captured(1).toInt());
		}
	}
	return QStringLiteral("%1 %2").arg(basename).arg(maxFound + 1);
}

void TimelineModel::setTimeline(const drawdance::Timeline &tl)
{
	beginResetModel();
	m_tracks.clear();
	int trackCount = tl.trackCount();
	m_tracks.reserve(trackCount);
	for(int i = 0; i < trackCount; ++i) {
		m_tracks.append(trackToModel(tl.trackAt(i)));
	}
	endResetModel();
}


QModelIndex
TimelineModel::index(int row, int column, const QModelIndex &parent) const
{
	return parent.isValid() ? QModelIndex() : createIndex(row, column);
}

QModelIndex TimelineModel::parent(const QModelIndex &child) const
{
	Q_UNUSED(child);
	return QModelIndex();
}

int TimelineModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_tracks.size();
}

int TimelineModel::columnCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_frameCount;
}

QVariant TimelineModel::data(const QModelIndex &index, int role) const
{
	if(index.parent().isValid()) {
		return QVariant();
	}

	int row = index.row();
	if(row < 0 || row >= m_tracks.size()) {
		return QVariant();
	}

	int column = index.column();
	if(column < 0 || column >= m_frameCount) {
		return QVariant();
	}

	switch(role) {
	case Qt::DisplayRole:
		return QStringLiteral("%1:%2").arg(row).arg(column);
	case TrackIdRole:
		return m_tracks[row].id;
	case TrackRole:
		return QVariant::fromValue(&m_tracks[row]);
	case KeyFrameRole:
		if(const TimelineKeyFrame *tkf =
			   m_tracks[row].searchKeyFrameByFrameIndex(column)) {
			return QVariant::fromValue(&tkf);
		} else {
			return QVariant();
		}
	case IsKeyFrameRole:
		return m_tracks[row].searchKeyFrameByFrameIndex(column) != nullptr;
	default:
		return QVariant();
	}
}

Qt::ItemFlags TimelineModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags itemFlags = QAbstractItemModel::flags(index);
	if(index.isValid()) {
		itemFlags.setFlag(Qt::ItemNeverHasChildren);
		bool keyFrame = index.data(IsKeyFrameRole).toBool();
		itemFlags.setFlag(Qt::ItemIsDragEnabled, keyFrame);
		itemFlags.setFlag(Qt::ItemIsDropEnabled, keyFrame);
	}
	return itemFlags;
}

void TimelineModel::setFrameCount(int frameCount)
{
	if(frameCount < m_frameCount) {
		beginRemoveColumns(QModelIndex(), frameCount, m_frameCount - 1);
		m_frameCount = frameCount;
		endRemoveColumns();
	} else if(frameCount > m_frameCount) {
		beginInsertColumns(QModelIndex(), m_frameCount, frameCount - 1);
		m_frameCount = frameCount;
		endInsertColumns();
	}
}

int TimelineModel::searchAvailableTrackId(
	const QSet<int> &takenIds, unsigned int contextId)
{
	for(int i = 0; i < DP_TRACK_ELEMENT_ID_MAX; ++i) {
		int id = DP_track_id_make(contextId, i);
		if(!takenIds.contains(id)) {
			return id;
		}
	}
	return 0;
}

TimelineTrack TimelineModel::trackToModel(const drawdance::Track &t)
{
	int keyFrameCount = t.keyFrameCount();
	QVector<TimelineKeyFrame> keyFrames;
	QHash<int, int> keyFrameIndexesByFrameIndex;
	keyFrames.reserve(keyFrameCount);
	keyFrameIndexesByFrameIndex.reserve(keyFrameCount);
	for(int i = 0; i < keyFrameCount; ++i) {
		int frameIndex = t.frameIndexAt(i);
		keyFrames.append(keyFrameToModel(t.keyFrameAt(i), frameIndex));
		keyFrameIndexesByFrameIndex.insert(frameIndex, i);
	}
	return {
		t.id(),		   t.title(), t.hidden(),
		t.onionSkin(), keyFrames, keyFrameIndexesByFrameIndex,
	};
}

TimelineKeyFrame
TimelineModel::keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex)
{
	TimelineKeyFrame tkf = {kf.layerId(), frameIndex, QColor(), kf.title(), {}};

	for(const DP_KeyFrameLayer &kfl : kf.layers()) {
		if(DP_key_frame_layer_hidden(&kfl)) {
			tkf.layerVisibility.insert(kfl.layer_id, false);
		} else if(DP_key_frame_layer_revealed(&kfl)) {
			tkf.layerVisibility.insert(kfl.layer_id, true);
		}
	}

	static QRegularExpression colorRe(QStringLiteral("\\A(#[0-9a-f]{6})!"));
	QRegularExpressionMatch match = colorRe.match(tkf.title);
	if(match.hasMatch()) {
		tkf.color = QColor(match.captured(1));
		tkf.title = tkf.title.mid(match.capturedEnd());
	}

	return tkf;
}

}
