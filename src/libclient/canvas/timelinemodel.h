// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TIMELINEMODEL_H
#define LIBCLIENT_CANVAS_TIMELINEMODEL_H
#include <QAbstractItemModel>
#include <QColor>
#include <QHash>
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
	int frameIndex;
	QColor color;
	QString title;
	QHash<int, bool> layerVisibility;

	bool hasSameContentAs(const TimelineKeyFrame &other) const
	{
		return layerId == other.layerId &&
			   layerVisibility == other.layerVisibility;
	}

	QString titleWithColor() const { return makeTitleWithColor(title, color); }

	static QString
	makeTitleWithColor(const QString &title, const QColor &color);
};

struct TimelineTrack final {
	int id;
	QString title;
	bool hidden;
	bool onionSkin;
	QVector<TimelineKeyFrame> keyFrames;
	QHash<int, int> keyFrameIndexesByFrameIndex;

	const TimelineKeyFrame *searchKeyFrameByFrameIndex(int frameIndex) const;
};

class TimelineModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	enum TimelineModelRole {
		TrackIdRole = Qt::UserRole + 1,
		TrackRole,
		KeyFrameRole,
		IsKeyFrameRole,
	};

	explicit TimelineModel(CanvasModel *canvas);

	void setAclState(AclState *aclState) { m_aclState = aclState; }

	const QVector<TimelineTrack> &tracks() const { return m_tracks; }

	const TimelineTrack *getTrackById(int trackId) const;

	int getAvailableTrackId() const;
	QString getAvailableTrackName(QString basename) const;

	void setTimeline(const drawdance::Timeline &tl);

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &child) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	void setFrameCount(int frameCount);

	const TimelineTrack &getTrackData(const QModelIndex &index) const;

	static int
	searchAvailableTrackId(const QSet<int> &takenIds, unsigned int contextId);

	static TimelineTrack trackToModel(const drawdance::Track &t);

	static TimelineKeyFrame
	keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex);

	QVector<TimelineTrack> m_tracks;
	AclState *m_aclState = nullptr;
	int m_frameCount;
};

}

#endif
