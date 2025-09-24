// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
#include <dpmsg/ids.h>
}
#include "libclient/canvas/acl.h"
#include "libclient/canvas/canvasmodel.h"
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


TimelineCamera::TimelineCamera(const drawdance::Camera &camera)
	: m_camera(camera)
	, m_title(camera.title())
{
}

bool TimelineCamera::operator<(const TimelineCamera &other) const
{
	int result = m_title.compare(other.m_title);
	if(result == 0) {
		return id() < other.id();
	} else {
		return result < 0;
	}
}


TimelineModel::TimelineModel(CanvasModel *canvas)
	: QObject{canvas}
	, m_tracks{}
	, m_aclState{nullptr}
{
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
	return searchAvailableIds(
		takenIds, DP_TRACK_ELEMENT_ID_MAX, DP_track_id_make);
}

int TimelineModel::getAvailableCameraId() const
{
	QSet<int> takenIds;
	takenIds.insert(0);
	for(const TimelineCamera &camera : m_cameras) {
		takenIds.insert(camera.id());
	}
	return searchAvailableIds(
		takenIds, DP_CAMERA_ELEMENT_ID_MAX, DP_camera_id_make);
}

QString TimelineModel::getAvailableTrackName(const QString &basename) const
{
	return searchAvailableName(basename, m_tracks.size(), [this](int i) {
		return std::ref(m_tracks[i].title);
	});
}

QString TimelineModel::getAvailableCameraName(const QString &basename) const
{
	return searchAvailableName(basename, m_cameras.size(), [this](int i) {
		return std::ref(m_cameras[i].title());
	});
}

void TimelineModel::setTimeline(const drawdance::Timeline &tl)
{
	m_tracks.clear();
	int trackCount = tl.trackCount();
	m_tracks.reserve(trackCount);
	m_lastFrameIndex = -1;
	for(int i = 0; i < trackCount; ++i) {
		m_tracks.append(trackToModel(tl.trackAt(i)));
		int trackLastFrameIndex = m_tracks[i].lastFrameIndex;
		if(trackLastFrameIndex > m_lastFrameIndex) {
			m_lastFrameIndex = trackLastFrameIndex;
		}
	}

	m_cameras.clear();
	int cameraCount = tl.cameraCount();
	m_cameras.reserve(cameraCount);
	for(int i = 0; i < cameraCount; ++i) {
		m_cameras.append(TimelineCamera(tl.cameraAt(i)));
	}
	std::sort(m_cameras.begin(), m_cameras.end());

	emit modelChanged();
}

int TimelineModel::searchAvailableIds(
	const QSet<int> &takenIds, int maxElementId,
	const std::function<int(unsigned int, int)> &makeId) const
{
	unsigned int localUserId = m_aclState ? m_aclState->localUserId() : 0u;
	int id = searchAvailableId(takenIds, localUserId, maxElementId, makeId);
	if(id != 0) {
		return id;
	}

	if(m_aclState && m_aclState->amOperator()) {
		id = searchAvailableId(takenIds, 0, maxElementId, makeId);
		if(id != 0) {
			return id;
		}

		for(unsigned int i = 255; i > 0; --i) {
			if(i != localUserId) {
				id = searchAvailableId(takenIds, i, maxElementId, makeId);
				if(id != 0) {
					return id;
				}
			}
		}
	}

	return 0;
}

int TimelineModel::searchAvailableId(
	const QSet<int> &takenIds, unsigned int contextId, int maxElementId,
	const std::function<int(unsigned int, int)> &makeId)
{
	for(int i = 0; i < maxElementId; ++i) {
		int id = makeId(contextId, i);
		if(!takenIds.contains(id)) {
			return id;
		}
	}
	return 0;
}

QString TimelineModel::searchAvailableName(
	QString basename, int elementCount,
	const std::function<const QString &(int)> &getElementTitleAt) const
{
	static QRegularExpression suffixNumRe(QStringLiteral("\\s*([0-9])+\\z"));
	QRegularExpressionMatch suffixNumMatch = suffixNumRe.match(basename);
	if(suffixNumMatch.hasMatch()) {
		basename = basename.mid(0, suffixNumMatch.capturedStart());
	}

	QRegularExpression re{QStringLiteral("\\A%1\\s*([0-9]+)\\z").arg(basename)};
	int maxFound = 0;
	for(int i = 0; i < elementCount; ++i) {
		QRegularExpressionMatch match = re.match(getElementTitleAt(i));
		if(match.hasMatch()) {
			maxFound = qMax(maxFound, match.captured(1).toInt());
		}
	}
	return QStringLiteral("%1 %2").arg(basename).arg(maxFound + 1);
}

TimelineTrack TimelineModel::trackToModel(const drawdance::Track &t)
{
	int keyFrameCount = t.keyFrameCount();
	QVector<TimelineKeyFrame> keyFrames;
	keyFrames.reserve(keyFrameCount);
	int lastFrameIndex = -1;
	for(int i = 0; i < keyFrameCount; ++i) {
		int frameIndex = t.frameIndexAt(i);
		keyFrames.append(keyFrameToModel(t.keyFrameAt(i), frameIndex));
		if(frameIndex > lastFrameIndex) {
			lastFrameIndex = frameIndex;
		}
	}
	return {t.id(),		   t.title(),	   t.hidden(),
			t.onionSkin(), lastFrameIndex, keyFrames};
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
