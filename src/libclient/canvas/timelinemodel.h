// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DP_TIMELINE_MODEL_H
#define DP_TIMELINE_MODEL_H

#include <QHash>
#include <QObject>
#include <QVector>

namespace drawdance {
class KeyFrame;
class LayerPropsList;
class Timeline;
class Track;
}

namespace canvas {

class AclState;
class CanvasModel;

struct TimelineKeyFrame final {
	int layerId;
	QString title;
	int frameIndex;
	QHash<int, bool> layerVisibility;

	bool hasSameContentAs(const TimelineKeyFrame &other) const
	{
		return layerId == other.layerId &&
			   layerVisibility == other.layerVisibility;
	}
};

struct TimelineTrack final {
	int id;
	QString title;
	bool hidden;
	bool onionSkin;
	QVector<TimelineKeyFrame> keyFrames;
};

class TimelineModel final : public QObject {
	Q_OBJECT
public:
	explicit TimelineModel(CanvasModel *canvas);

	void setAclState(AclState *aclState) { m_aclState = aclState; }

	const QVector<TimelineTrack> &tracks() { return m_tracks; }

	int getAvailableTrackId() const;
	QString getAvailableTrackName(QString basename) const;

public slots:
	void setTimeline(const drawdance::Timeline &tl);

signals:
	void tracksChanged();

private:
	static TimelineTrack trackToModel(const drawdance::Track &t);

	static TimelineKeyFrame
	keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex);

	QVector<TimelineTrack> m_tracks;
	AclState *m_aclState;
};

}

#endif
