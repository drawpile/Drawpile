/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2016 Calle Laakkonen

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
#ifndef PLAYBACKCONTROLLER_H
#define PLAYBACKCONTROLLER_H

#include "../shared/net/message.h"
#include "index.h"

#include <QObject>
#include <QPointer>
#include <QScopedPointer>

class QTimer;
class QStringList;

class VideoExporter;

namespace canvas {
	class CanvasModel;
}

namespace recording {

class Reader;
class IndexLoader;
class IndexBuilder;

/**
 * @brief The playback controller is responsible for the state of the recording playback
 *
 * This View is implemented in QML.
 * Note: part of the logic (relating to the opening of new dialogs) is still in PlaybackDialog.
 */
class PlaybackController : public QObject
{
	Q_PROPERTY(bool playing READ isPlaying WRITE setPlaying NOTIFY playbackToggled)
	Q_PROPERTY(qreal speedFactor READ speedFactor WRITE setSpeedFactor NOTIFY speedFactorChanged)
	Q_PROPERTY(qint64 progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(qint64 maxProgress READ maxProgress CONSTANT)
	Q_PROPERTY(qreal indexBuildProgress READ indexBuildProgress NOTIFY indexBuildProgressed)
	Q_PROPERTY(bool canSaveFrame READ canSaveFrame NOTIFY canSaveFrameChanged)
	Q_PROPERTY(int currentExportFrame READ currentExportFrame NOTIFY exportedFrame)
	Q_PROPERTY(QString currentExportTime READ currentExportTime NOTIFY exportedFrame)
	Q_PROPERTY(bool autosave READ isAutosaving WRITE setAutosave NOTIFY autosaveChanged)
	Q_PROPERTY(QStringList markers READ getMarkers NOTIFY markersChanged)
	Q_PROPERTY(bool stopOnMarkers READ stopOnMarkers WRITE setStopOnMarkers NOTIFY stopOnMarkersChanged)

	Q_OBJECT
public:
	PlaybackController(canvas::CanvasModel *canvas, recording::Reader *reader, QObject *parent = 0);
	~PlaybackController();

	bool isPlaying() const { return m_play; }

	qreal speedFactor() const { return m_speedFactor; }
	void setSpeedFactor(qreal value) { m_speedFactor = qMax(0.01, value); emit speedFactorChanged(m_speedFactor); }

	int indexPosition() const;
	int maxIndexPosition() const;

	qint64 progress() const;
	qint64 maxProgress() const;
	qreal indexBuildProgress() const { return m_indexBuildProgress; }

	bool canSaveFrame() const;
	bool isExporting() const { return m_exporter != nullptr; }
	int currentExportFrame() const;
	QString currentExportTime() const;

	void setAutosave(bool as) { m_autosave = as; emit autosaveChanged(); }
	bool isAutosaving() const { return m_autosave; }

	QStringList getMarkers() const;

	bool stopOnMarkers() const { return m_stopOnMarkers; }
	void setStopOnMarkers(bool s) { if(s!= m_stopOnMarkers) { m_stopOnMarkers = s; emit stopOnMarkersChanged(); } }

	QString recordingFilename() const;

	void startVideoExport(VideoExporter *exporter);

	bool hasIndex() const { return !m_indexloader.isNull(); }

	int indexThumbnailCount() const;
	QImage getIndexThumbnail(int idx) const;

signals:
	void commandRead(protocol::MessagePtr msg);
	void speedFactorChanged(qreal value);
	void progressChanged(qint64 progress);
	void indexPositionChanged(int pos);
	void playbackToggled(bool play);
	void stopOnMarkersChanged();
	void endOfFileReached();
	void markersChanged();
	void canSaveFrameChanged(bool);
	void autosaveChanged();
	void exportedFrame();

	void exportStarted();
	void exportEnded();
	void exportError(const QString &message);

	void indexLoadError(const QString &message, bool canRetry);
	void indexBuildProgressed(qreal progress);
	void indexLoaded();


public slots:
	void setPlaying(bool play);
	void nextCommand();
	void nextSequence();
	void prevSequence();

	void jumpToMarker(int index);

	void jumpTo(int pos);

	void loadIndex();
	void buildIndex();

	void setPauses(bool pauses);

	void exportFrame(int count=1);
	void stopExporter();

private slots:
	void exporterReady();
	void exporterError(const QString &message);
	void exporterFinished();

private:
	void nextCommands(int stepCount);
	void jumpToSnapshot(int idx);
	void updateIndexPosition();
	bool waitForExporter();

	QString indexFileName() const;

	Reader *m_reader;
	QScopedPointer<IndexLoader> m_indexloader;
	QPointer<IndexBuilder> m_indexbuilder;
	VideoExporter *m_exporter;

	canvas::CanvasModel *m_canvas;

	QTimer *m_timer;

	bool m_play;
	bool m_exporterReady;
	bool m_waitedForExporter;
	bool m_autosave;

	bool m_stopOnMarkers;
	qreal m_maxInterval;
	qreal m_speedFactor;

	qreal m_indexBuildProgress;
};

}

#endif
