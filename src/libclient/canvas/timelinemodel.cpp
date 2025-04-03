// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/key_frame.h>
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
	int prefix = (m_aclState ? int(m_aclState->localUserId()) : 0) << 8;
	QSet<int> takenIds;
	for(const TimelineTrack &track : m_tracks) {
		if((track.id & 0xff00) == prefix) {
			takenIds.insert(track.id);
		}
	}

	for(int i = 0; i < 256; ++i) {
		int id = prefix | i;
		if(!takenIds.contains(id)) {
			return id;
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
	m_tracks.clear();
	int trackCount = tl.trackCount();
	m_tracks.reserve(trackCount);
	for(int i = 0; i < trackCount; ++i) {
		m_tracks.append(trackToModel(tl.trackAt(i)));
	}
	emit tracksChanged();
}

TimelineTrack TimelineModel::trackToModel(const drawdance::Track &t)
{
	int keyFrameCount = t.keyFrameCount();
	QVector<TimelineKeyFrame> keyFrames;
	keyFrames.reserve(keyFrameCount);
	for(int i = 0; i < keyFrameCount; ++i) {
		keyFrames.append(keyFrameToModel(t.keyFrameAt(i), t.frameIndexAt(i)));
	}
	return {t.id(), t.title(), t.hidden(), t.onionSkin(), keyFrames};
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
