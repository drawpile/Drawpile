// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TIMELINEMODEL_H
#define LIBCLIENT_CANVAS_TIMELINEMODEL_H
#include "libclient/drawdance/camera.h"
#include <QColor>
#include <QHash>
#include <QObject>
#include <QVector>
#include <functional>

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
	int lastFrameIndex;
	QVector<TimelineKeyFrame> keyFrames;
};

class TimelineCamera final {
public:
	TimelineCamera(const drawdance::Camera &camera);

	bool hidden() const { return m_camera.hidden(); }

	int id() const { return m_camera.id(); }

	unsigned int flags() const { return m_camera.flags(); }

	int interpolation() const { return m_camera.interpolation(); }

	double effectiveFramerate() const { return m_camera.effectiveFramerate(); }
	bool frameratesValid() const { return m_camera.frameratesValid(); }

	int rangeFirst() const { return m_camera.rangeFirst(); }
	int rangeLast() const { return m_camera.rangeLast(); }
	bool rangeValid() const { return m_camera.rangeValid(); }

	QSize outputSize() const { return m_camera.outputSize(); }
	bool outputValid() const { return m_camera.outputValid(); }

	QRect viewport() const { return m_camera.viewport(); }
	bool viewportValid() const { return m_camera.viewportValid(); }

	const QString &title() const { return m_title; }

	bool operator<(const TimelineCamera &other) const;

private:
	drawdance::Camera m_camera;
	QString m_title;
};

class TimelineModel final : public QObject {
	Q_OBJECT
public:
	explicit TimelineModel(CanvasModel *canvas);

	void setAclState(AclState *aclState) { m_aclState = aclState; }

	const QVector<TimelineTrack> &tracks() const { return m_tracks; }
	const QVector<TimelineCamera> &cameras() const { return m_cameras; }

	int lastFrameIndex() const { return m_lastFrameIndex; }

	const TimelineTrack *getTrackById(int trackId) const;

	int getAvailableTrackId() const;
	int getAvailableCameraId() const;
	QString getAvailableTrackName(const QString &basename) const;
	QString getAvailableCameraName(const QString &basename) const;

public slots:
	void setTimeline(const drawdance::Timeline &tl);

signals:
	void modelChanged();

private:
	int searchAvailableIds(
		const QSet<int> &takenIds, int maxElementId,
		const std::function<int(unsigned int, int)> &makeId) const;

	static int searchAvailableId(
		const QSet<int> &takenIds, unsigned int contextId, int maxElementId,
		const std::function<int(unsigned int, int)> &makeId);

	QString searchAvailableName(
		QString basename, int elementCount,
		const std::function<const QString &(int)> &getElementTitleAt) const;

	static TimelineTrack trackToModel(const drawdance::Track &t);

	static TimelineKeyFrame
	keyFrameToModel(const drawdance::KeyFrame &kf, int frameIndex);

	QVector<TimelineTrack> m_tracks;
	QVector<TimelineCamera> m_cameras;
	AclState *m_aclState;
	int m_lastFrameIndex = -1;
};

}

#endif
