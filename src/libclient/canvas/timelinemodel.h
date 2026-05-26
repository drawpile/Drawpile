// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TIMELINEMODEL_H
#define LIBCLIENT_CANVAS_TIMELINEMODEL_H
#include <QAbstractItemModel>
#include <QColor>
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
	bool moveLock;
	int lastFrameIndex;
	QVector<TimelineKeyFrame> keyFrames;
};

class TimelineModel final : public QObject {
	Q_OBJECT
public:
	explicit TimelineModel(CanvasModel *canvas);

	void setAclState(AclState *aclState) { m_aclState = aclState; }

	const QVector<TimelineTrack> &tracks() const { return m_tracks; }

	int lastFrameIndex() const { return m_lastFrameIndex; }

	const TimelineTrack *getTrackById(int trackId) const;

	int getAvailableTrackId() const;
	QString getAvailableTrackName(QString basename) const;

	void setSelectedTrackIdToRestore(int trackId)
	{
		m_selectedTrackIdToRestore = trackId;
	}

	void setSelectedFrameIndexToRestore(int frameIndex)
	{
		m_selectedFrameIndexToRestore = frameIndex;
	}

public slots:
	void setTimeline(const drawdance::Timeline &tl);

signals:
	void tracksChanged();
	void restoreSelectedTrackId(int trackId);
	void restoreSelectedFrameIndex(int frameIndex);

private:
	static int
	searchAvailableTrackId(const QSet<int> &takenIds, unsigned int contextId);

	static TimelineTrack trackToModel(const drawdance::Track &t);

	static TimelineKeyFrame
	keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex);

	QVector<TimelineTrack> m_tracks;
	AclState *m_aclState = nullptr;
	int m_lastFrameIndex = -1;
	int m_selectedTrackIdToRestore = 0;
	int m_selectedFrameIndexToRestore = -1;
};

// Dummy model to implement multi-frame selection. Doesn't actually return any
// useful data, just keeps track of the number of tracks and frames.
class TimelineItemModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	TimelineItemModel(QObject *parent = nullptr);

	void setTrackCount(int trackCount);
	void setFrameCount(int frameCount);

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &child) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

private:
	int m_trackCount = 0;
	int m_frameCount = 0;
};

}

#endif
