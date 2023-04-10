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

class CanvasModel;

struct TimelineKeyFrame final {
	int layerId;
	QString title;
	int frameIndex;
	QHash<int, bool> layerVisibility;
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

	const CanvasModel *canvas() { return m_canvas; }
	const QVector<TimelineTrack> &tracks() { return m_tracks; }

	int frameCount() const { return m_frameCount; }
	void setFrameCount(int frameCount);

	bool isManualMode() { return m_manualMode; }
	void setManualMode(bool manualMode) { m_manualMode = manualMode; }

	uint8_t localUserId() const;

	int getAvailableTrackId() const;
	QString getAvailableTrackName(QString basename) const;

public slots:
	void setLayers(const drawdance::LayerPropsList &lpl);
	void setTimeline(const drawdance::Timeline &tl);

signals:
	void layersChanged();
	void tracksChanged();
	void frameCountChanged(int frameCount);

private:
	static TimelineTrack trackToModel(const drawdance::Track &t);
	static TimelineKeyFrame
	keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex);

	bool m_manualMode;
	CanvasModel *m_canvas;
	QVector<TimelineTrack> m_tracks;
	int m_frameCount;
};

}

#endif
