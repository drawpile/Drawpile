/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "playbackcontroller.h"
#include "indexbuilder.h"
#include "indexloader.h"

#include "export/videoexporter.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"
#include "net/internalmsg.h"

#include "canvas/statetracker.h"
#include "canvas/canvasmodel.h"

#include <QStringList>
#include <QThread>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>

namespace recording {

PlaybackController::PlaybackController(canvas::CanvasModel *canvas, Reader *reader, QObject *parent)
	: QObject(parent),
	  m_reader(reader), m_indexloader(nullptr), m_exporter(nullptr), m_canvas(canvas),
	  m_play(false), m_exporterReady(false), m_waitedForExporter(false), m_autosave(true),
	  m_maxInterval(60.0), m_speedFactor(1.0),
	  m_indexBuildProgress(0)
{
	reader->setParent(this);

	m_autoplayTimer = new QTimer(this);
	m_autoplayTimer->setSingleShot(true);
	connect(m_autoplayTimer, &QTimer::timeout, this, &PlaybackController::nextCommand);

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("playback");
	m_stopOnMarkers = cfg.value("stoponmarkers", true).toBool();

	// Automatically take the first step
	m_autoplayTimer->start(0);

	connect(this, &PlaybackController::endOfFileReached, [this]() { setPlaying(false); });
	connect(canvas->stateTracker(), &canvas::StateTracker::sequencePoint, this, &PlaybackController::onSequencePoint);
}

PlaybackController::~PlaybackController()
{
	// Remember some settings
	QSettings cfg;
	cfg.beginGroup("playback");

	cfg.setValue("stoponmarkers", m_stopOnMarkers);
}

qint64 PlaybackController::progress() const
{
	return m_reader->currentPosition();
}

qint64 PlaybackController::maxProgress() const
{
	if(m_reader->isCompressed())
		return -1;
	return m_reader->filesize();
}

int PlaybackController::indexPosition() const
{
	return m_reader->currentIndex();
}

int PlaybackController::maxIndexPosition() const
{
	if(!m_indexloader)
		return -1;
	return m_indexloader->index().actionCount();
}

void PlaybackController::setPauses(bool pauses)
{
	if(pauses)
		m_maxInterval = 60.0;
	else
		m_maxInterval = 0;
}

void PlaybackController::setPlaying(bool play)
{
	if(play != m_play) {
		m_play = play;
		if(play)
			m_autoplayTimer->start(0);
		else
			m_autoplayTimer->stop();

		emit playbackToggled(play);
	}
}

void PlaybackController::nextCommand()
{
	int c = 1;
	if(m_play)
		c = qMax(1, qRound(m_speedFactor));
	nextCommands(c);
}

void PlaybackController::nextCommands(int stepCount)
{
	Q_ASSERT(stepCount>0);

	if(waitForExporter()) {
		m_waitedForExporter = true;
		return;
	}

	while(stepCount-->0) {
		MessageRecord next = m_reader->readNext();

		switch(next.status) {
		case MessageRecord::OK: {
			if(next.message->type() == protocol::MSG_INTERVAL) {
				if(m_play) {
					// Autoplay mode: pause for the given interval
					expectSequencePoint(next.message.cast<protocol::Interval>().milliseconds() / m_speedFactor);
					return;

				} else {
					// Manual mode: skip interval
					++stepCount;
				}

			} else {
				if(next.message->type() == protocol::MSG_MARKER) {
					emit markerEncountered(next.message.cast<protocol::Marker>().text());
					if(m_stopOnMarkers)
						setPlaying(false);
				}

				m_canvas->handleCommand(protocol::MessagePtr::fromNullable(next.message));
			}
			break;
		}
		case MessageRecord::INVALID:
			qWarning("Unrecognized command %d of length %d", next.invalid_type, next.invalid_len);
			break;
		case MessageRecord::END_OF_RECORDING:
			emit endOfFileReached();
			break;
		}
	}

	expectSequencePoint(qMax(1.0, 33.0 / m_speedFactor) + 0.5);
}

void PlaybackController::expectSequencePoint(int interval)
{
	// We have just queued a sequence of messages for execution.
	// Add a trailing SequencePoint and start expecting its roundtrip.
	m_sequenceTimer.restart();
	m_canvas->handleCommand(protocol::ClientInternal::makeSequencePoint(interval));
}

void PlaybackController::onSequencePoint(int interval)
{
	// A message sequence has just been fully executed.
	const int maxInterval = m_maxInterval * 1000;
	if(m_exporter && m_autosave) {
		const int writeFrames = qBound(1, int(interval / 1000.0 * m_exporter->fps()), maxInterval);
		exportFrame(writeFrames);
	}

	updateIndexPosition();

	// Continue playback (if in autoplay mode)
	if(m_play) {
		qint64 elapsed = m_sequenceTimer.elapsed();
		m_autoplayTimer->start(qBound(1, int(interval / m_speedFactor - elapsed), maxInterval));
	}
}

void PlaybackController::nextSequence()
{
	if(waitForExporter())
		return;

	// Play back until either an undopoint or end-of-file is reached
	bool loop=true;
	while(loop) {
		MessageRecord next = m_reader->readNext();
		switch(next.status) {
		case MessageRecord::OK:
			// skip intervals
			if(next.message->type() != protocol::MSG_INTERVAL) {
				m_canvas->handleCommand(protocol::MessagePtr::fromNullable(next.message));
				if(next.message->type() == protocol::MSG_UNDOPOINT)
					loop = false;
			}
			break;
		case MessageRecord::INVALID:
			qWarning("Unrecognized command %d of length %d", next.invalid_type, next.invalid_len);
			break;
		case MessageRecord::END_OF_RECORDING:
			emit endOfFileReached();
			loop = false;
			break;
		}
	}

	expectSequencePoint(0);
}

void PlaybackController::prevSequence()
{
	if(!m_indexloader) {
		qWarning("prevSequence: index not loaded!");
		return;
	}
	const Index &index = m_indexloader->index();
	jumpTo(index.entry(index.findPreviousStop(m_reader->currentIndex())).index);
}

void PlaybackController::jumpTo(int pos)
{
	if(!m_indexloader) {
		qWarning("jumpTo(%d): index not loaded!", pos);
		return;
	}

	if(pos == m_reader->currentIndex())
		return;

	if(waitForExporter())
		return;

	// If the target position is behind current position or sufficiently far ahead, jump
	// to the closest snapshot point first
	if(pos < m_reader->currentIndex() || pos - m_reader->currentIndex() > 500) {
		const Index &index = m_indexloader->index();
		int snap = index.findClosestSnapshot(pos);

		// When jumping forward, don't restore the snapshot if the snapshot is behind
		// the current position
		if(pos < m_reader->currentIndex() || index.entries().at(snap).pos > quint32(m_reader->currentIndex()))
			jumpToSnapshot(snap);
	}

	// Now the current position is somewhere before the target position: replay commands
	while(m_reader->currentIndex() < pos && !m_reader->isEof()) {
		MessageRecord next = m_reader->readNext();
		switch(next.status) {
		case MessageRecord::OK:
			// skip intervals
			if(next.message->type() != protocol::MSG_INTERVAL) {
				m_canvas->handleCommand(protocol::MessagePtr::fromNullable(next.message));
			}
			break;
		case MessageRecord::INVALID:
			qWarning("Unrecognized command %d of length %d", next.invalid_type, next.invalid_len);
			break;
		case MessageRecord::END_OF_RECORDING:
			emit endOfFileReached();
			break;
		}
	}

	expectSequencePoint(0);
}

void PlaybackController::jumpToSnapshot(int idx)
{
	Q_ASSERT(m_indexloader);

	StopEntry se = m_indexloader->index().entry(idx);
	canvas::StateSavepoint savepoint = m_indexloader->loadSavepoint(idx);

	if(!savepoint) {
		qWarning("error loading savepoint");
		return;
	}

	m_reader->seekTo(se.index, se.pos);
	m_canvas->stateTracker()->resetToSavepoint(savepoint);
	updateIndexPosition();
}

void PlaybackController::jumpToMarker(int index)
{
	if(!m_indexloader)
		return;

	const QVector<MarkerEntry> &markers = m_indexloader->index().markers();
	if(index<0 || index >= markers.size()) {
		qWarning("markers()[%d] does not exist", index);
		return;
	}

	jumpTo(m_indexloader->index().entry(markers.at(index).stop).index);
}

void PlaybackController::updateIndexPosition()
{
	emit progressChanged(progress());
	if(m_indexloader)
		emit indexPositionChanged(m_reader->currentIndex());
}

QString PlaybackController::indexFileName() const
{
	QString name = m_reader->filename();
	int suffix = name.lastIndexOf('.');
	return name.left(suffix) + ".dpidx";
}

/**
 * @brief (Re)build the index file
 */
void PlaybackController::buildIndex()
{
	if(!m_indexbuilder.isNull()) {
		qWarning("Index builder already running!");
		return;
	}

	QThread *thread = new QThread;
	m_indexbuilder = new IndexBuilder(m_reader->filename(), indexFileName());
	m_indexbuilder->moveToThread(thread);

	const qreal filesize = m_reader->filesize();
	connect(m_indexbuilder.data(), &IndexBuilder::progress, this, [this, filesize](int progress) {
		m_indexBuildProgress = progress / filesize;
		emit indexBuildProgressed(m_indexBuildProgress);
	});

	connect(m_indexbuilder.data(), &IndexBuilder::done, this, [this](bool ok, const QString &error) {
		if(!ok) {
			m_indexBuildProgress = 0;
			emit indexBuildProgressed(0);
			emit indexLoadError(error, true);
		} else {
			loadIndex();
		}
	});

	connect(thread, &QThread::started, m_indexbuilder.data(), &IndexBuilder::run);
	connect(m_indexbuilder.data(), &IndexBuilder::done, thread, &QThread::quit);
	connect(m_indexbuilder.data(), &IndexBuilder::done, m_indexbuilder.data(), &QThread::deleteLater);
	connect(thread, &QThread::finished, thread, &QThread::deleteLater);

	thread->start();
}

void PlaybackController::loadIndex()
{
	if(m_reader->isCompressed()) {
		emit indexLoadError(tr("Cannot index compressed recordings."), false);
		return;
	}

	QFileInfo indexfile(indexFileName());
	if(!indexfile.exists()) {
		emit indexLoadError(tr("Index not yet generated"), true);
		return;
	}

	m_indexloader.reset(new IndexLoader(m_reader->filename(), indexFileName()));
	if(!m_indexloader->open()) {
		m_indexloader.reset();
		emit indexLoadError(tr("Error loading index!"), true);
		return;
	}

	emit markersChanged();

	emit indexLoaded();
}

QStringList PlaybackController::getMarkers() const
{
	QStringList markers;
	if(m_indexloader) {
		for(const MarkerEntry &m : m_indexloader->index().markers()) {
			markers << m.title;
		}
	}
	return markers;
}

int PlaybackController::indexThumbnailCount() const
{
	if(m_indexloader)
		return m_indexloader->thumbnailsAvailable();
	else
		return -1;
}

QImage PlaybackController::getIndexThumbnail(int idx) const
{
	if(!m_indexloader)
		return QImage();

	return m_indexloader->loadThumbnail(idx);
}

QString PlaybackController::recordingFilename() const
{
	return m_reader->filename();
}

bool PlaybackController::canSaveFrame() const
{
	return m_exporter && m_exporterReady;
}

int PlaybackController::currentExportFrame() const
{
	if(!m_exporter)
		return 0;
	return m_exporter->frame();
}

QString PlaybackController::currentExportTime() const
{
	if(!m_exporter)
		return QString();

	float time = m_exporter->time();
	int minutes = time / 60;
	if(minutes>0) {
		time = fmod(time, 60.0);
		return tr("%1 m. %2 s.").arg(minutes).arg(time, 0, 'f', 2);
	} else {
		return tr("%1 s.").arg(time, 0, 'f', 2);
	}
}

void PlaybackController::startVideoExport(VideoExporter *exporter)
{
	Q_ASSERT(!m_exporter);
	Q_ASSERT(exporter);
	m_exporter = exporter;
	m_exporter->setParent(this);

	m_exporterReady = false;
	m_waitedForExporter = false;

	connect(m_exporter, &VideoExporter::exporterReady, this, &PlaybackController::exporterReady, Qt::QueuedConnection);
	connect(m_exporter, SIGNAL(exporterError(QString)), this, SLOT(exporterError(QString)), Qt::QueuedConnection);
	connect(m_exporter, SIGNAL(exporterFinished()), this, SLOT(exporterFinished()), Qt::QueuedConnection);

	m_exporter->start();
	emit exportStarted();
	emit canSaveFrameChanged(canSaveFrame());
}

void PlaybackController::exporterReady()
{
	m_exporterReady = true;
	emit exportedFrame();

	// Stop exporting after saving the last frame
	if(m_reader->isEof()) {
		m_exporter->finish();
	}
	// Tried to proceed to next frame while exporter was busy
	else if(m_waitedForExporter) {
		m_waitedForExporter = false;
		nextCommand();
	}

	emit canSaveFrameChanged(canSaveFrame());
}

void PlaybackController::exporterError(const QString &message)
{
	// Stop playback on error
	if(isPlaying())
		setPlaying(false);

	emit exportError(message);

	exporterFinished();
}

void PlaybackController::exporterFinished()
{
	delete m_exporter;
	m_exporter = nullptr;

	emit exportEnded();
}

void PlaybackController::exportFrame(int count)
{
	if(count<1)
		count = 1;

	if(m_exporter) {
		QImage img = m_canvas->toImage();
		if(!img.isNull()) {
			Q_ASSERT(m_exporterReady);
			m_exporterReady = false;
			emit canSaveFrameChanged(canSaveFrame());
			m_exporter->saveFrame(img, count);
		}
	} else {
		qWarning("exportFrame(%d): exported not active!", count);
	}
}

void PlaybackController::stopExporter()
{
	if(isPlaying())
		setPlaying(false);
	if(m_exporter)
		m_exporter->finish();
	else
		qWarning("stopExporter(): exported not active!");
}

bool PlaybackController::waitForExporter()
{
	return m_exporter && !m_exporterReady;
}

}

