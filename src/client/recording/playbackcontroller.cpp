/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#include "utils/iconprovider.h"
#include "export/videoexporter.h"

#include "../shared/record/reader.h"
#include "../shared/net/recording.h"

#include "notifications.h"
#include "canvas/statetracker.h"
#include "canvas/canvasmodel.h"

#include <QStringList>
#include <QThreadPool>
#include <QFileInfo>
#include <QSettings>
#include <QTimer>
#include <QDebug>
#include <QMessageBox> // TODO move away from here

namespace recording {

PlaybackController::PlaybackController(canvas::CanvasModel *canvas, Reader *reader, QObject *parent)
	: QObject(parent),
	  m_reader(reader), m_indexloader(nullptr), m_exporter(nullptr), m_canvas(canvas),
	  m_play(false), m_exporterReady(false), m_waitedForExporter(false), m_autosave(true),
	  m_maxInterval(60.0), m_speedFactor(1.0),
	  m_indexBuildProgress(0)
{
	m_timer = new QTimer(this);
	m_timer->setSingleShot(true);
	connect(m_timer, &QTimer::timeout, this, &PlaybackController::nextCommand);

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("playback");
	m_stopOnMarkers = cfg.value("stoponmarkers", true).toBool();

	// Automatically take the first step
	m_timer->start(0);

	connect(this, &PlaybackController::endOfFileReached, [this]() { setPlaying(false); });
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
			m_timer->start(0);
		else
			m_timer->stop();

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

	MessageRecord next = m_reader->readNext();

	int writeFrames = 1;

	switch(next.status) {
	case MessageRecord::OK: {
		protocol::MessagePtr msg(next.message);

		if(msg->type() == protocol::MSG_INTERVAL) {
			if(m_play) {
				// Autoplay mode: pause for the given interval
				int interval = msg.cast<protocol::Interval>().milliseconds();
				int maxinterval = m_maxInterval * 1000;
				m_timer->start(qMin(maxinterval, int(interval / m_speedFactor)));

				if(m_exporter && m_autosave) {
					int pauseframes = qRound(qMin(interval, maxinterval) / 1000.0 *  m_exporter->fps());
					if(pauseframes>0)
						writeFrames = pauseframes;
				}

			} else {
				// Manual mode: skip interval
				nextCommands(1);
				return;
			}
		} else {
			if(m_play) {
				if(msg->type() == protocol::MSG_MARKER && m_stopOnMarkers) {
					setPlaying(false);
					notification::playSound(notification::Event::MARKER);
				} else {
					if(stepCount==1)
						m_timer->start(int(qMax(1.0, 33.0 / m_speedFactor) + 0.5));
				}
			}

			emit commandRead(msg);
		}
		break;
	}
	case MessageRecord::INVALID:
		qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
		if(m_play)
			m_timer->start(1);
		break;
	case MessageRecord::END_OF_RECORDING:
		emit endOfFileReached();
		break;
	}

	if(stepCount>1) {
		nextCommands(stepCount-1);

	} else {
		if(m_exporter && m_autosave)
			exportFrame(writeFrames);

		updateIndexPosition();
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
			if(next.message->type() == protocol::MSG_INTERVAL) {
				// skip intervals
				delete next.message;
			} else {
				emit commandRead(protocol::MessagePtr(next.message));
				if(next.message->type() == protocol::MSG_UNDOPOINT)
					loop = false;
			}
			break;
		case MessageRecord::INVALID:
			qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
			break;
		case MessageRecord::END_OF_RECORDING:
			emit endOfFileReached();
			loop = false;
			break;
		}
	}

	if(m_exporter && m_autosave)
		exportFrame();

	updateIndexPosition();
}

void PlaybackController::prevSequence()
{
	if(!m_indexloader) {
		qWarning("prevSequence: index not loaded!");
		return;
	}
	const Index &index = m_indexloader->index();

	// Find previous index entry
	int i=0;
	const unsigned int pos = m_reader->currentIndex();
	for(const IndexEntry &e : index.entries()) {
		if(e.end >= pos) {
			i = qMax(0, i-1);
			break;
		}
		++i;
	}

	if(i < index.entries().size())
		jumpTo(index.entry(i).start - 1);
}

void PlaybackController::jumpTo(int pos)
{
	Q_ASSERT(m_indexloader);

	if(pos == m_reader->currentIndex())
		return;

	if(waitForExporter())
		return;

	// If the target position is behind current position or sufficiently far ahead, jump
	// to the closest snapshot point first
	if(pos < m_reader->currentIndex() || pos - m_reader->currentIndex() > 500) {
		int seIdx=0;
		const Index &index = m_indexloader->index();
		for(int i=1;i<index.snapshots().size();++i) {
			const SnapshotEntry &next = index.snapshots().at(i);
			if(int(next.pos) > pos)
				break;

			seIdx = i;
		}

		// When jumping forward, don't restore the snapshot if the snapshot is behind
		// the current position
		if(pos < m_reader->currentIndex() || index.snapshots().at(seIdx).pos > quint32(m_reader->currentIndex()))
			jumptToSnapshot(seIdx);
	}

	// Now the current position is somewhere before the target position: replay commands
	while(m_reader->currentIndex() < pos && !m_reader->isEof()) {
		MessageRecord next = m_reader->readNext();
		switch(next.status) {
		case MessageRecord::OK:
			if(next.message->type() == protocol::MSG_INTERVAL) {
				// skip intervals
				delete next.message;
			} else {
				emit commandRead(protocol::MessagePtr(next.message));
			}
			break;
		case MessageRecord::INVALID:
			qWarning() << "Unrecognized command " << next.error.type << "of length" << next.error.len;
			break;
		case MessageRecord::END_OF_RECORDING:
			emit endOfFileReached();
			break;
		}
	}

	if(m_exporter && m_autosave)
		exportFrame();

	updateIndexPosition();
}

void PlaybackController::jumptToSnapshot(int idx)
{
	Q_ASSERT(m_indexloader);

	SnapshotEntry se = m_indexloader->index().snapshots().at(idx);
	canvas::StateSavepoint savepoint = m_indexloader->loadSavepoint(idx, m_canvas->stateTracker());

	if(!savepoint) {
		qWarning() << "error loading savepoint";
		return;
	}

	m_reader->seekTo(se.pos, se.stream_offset);
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

	jumpTo(markers.at(index).pos);
}

void PlaybackController::prevMarker()
{
	if(!m_indexloader)
		return;
	recording::MarkerEntry e = m_indexloader->index().prevMarker(qMax(0, m_reader->currentIndex()));
	if(e.pos>0) {
		jumpTo(e.pos);
	}
}

void PlaybackController::nextMarker()
{
	if(!m_indexloader)
		return;
	recording::MarkerEntry e = m_indexloader->index().nextMarker(qMax(0, m_reader->currentIndex()));
	if(e.pos>0) {
		jumpTo(e.pos);
	}
}

void PlaybackController::addMarker(const QString &text)
{
	if(!m_indexloader)
		return;

	m_indexloader->index().addMarker(m_reader->currentPosition(), m_reader->currentIndex(), text);
	emit markersChanged();
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

	m_indexbuilder = new IndexBuilder(m_reader->filename(), indexFileName());

	const qreal filesize = m_reader->filesize();
	connect(m_indexbuilder, &IndexBuilder::progress, [this, filesize](int progress) {
		m_indexBuildProgress = progress / filesize;
		emit indexBuildProgressed(m_indexBuildProgress);
	});

	connect(m_indexbuilder, &IndexBuilder::done, [this](bool ok, const QString &error) {
		if(!ok) {
			m_indexBuildProgress = 0;
			emit indexBuildProgressed(0);
			emit indexLoadError(error, true);
		} else {
			loadIndex();
		}
	});

	QThreadPool::globalInstance()->start(m_indexbuilder);
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

	m_indexloader = new IndexLoader(m_reader->filename(), indexFileName());
	if(!m_indexloader->open()) {
		delete m_indexloader;
		m_indexloader = nullptr;
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

IndexVector PlaybackController::getSilencedEntries() const
{
	if(!m_indexloader)
		return IndexVector();
	return m_indexloader->index().silencedEntries();
}

IndexVector PlaybackController::getNewMarkers() const
{
	if(!m_indexloader)
		return IndexVector();
	return m_indexloader->index().newMarkers();
}

/**
 * The returned object is used in QML to build the index scene.
 * @return
 */
QVariantMap PlaybackController::getIndexItems() const
{
	if(!m_indexloader)
		return QVariantMap();

	QVariantList items, contextNames;

	int contextNameIdx[255];
	contextNameIdx[0] = 0;
	for(int i=1;i<255;++i)
		contextNameIdx[i] = -1;
	contextNames.append(QString());

	// Icons for IndexTypes
	QString typeIcons[] = {
		QString(),
		QStringLiteral("flag-red"),
		QString(),
		QStringLiteral("list-add"),
		QStringLiteral("list-remove"),
		QStringLiteral("edit-paste"),
		QStringLiteral("draw-brush"),
		QStringLiteral("draw-text"),
		QStringLiteral("chat"), // TODO
		QStringLiteral("media-playback-pause"),
		QString(), // TODO
		QStringLiteral("edit-undo"),
		QStringLiteral("edit-redo"),
		QStringLiteral("draw-rectangle"),
	};

	// Get index items and context names.
	// Context names are added to the list in the order they appear
	const Index &index = m_indexloader->index();
	for(const IndexEntry &e : index.entries()) {

		// Check if this context ID has already appeared
		// and get it's name if it hasn't
		int nameIdx = 0;
		for(;nameIdx<contextNames.size();++nameIdx)
			if(contextNameIdx[nameIdx] == e.context_id)
				break;
		if(nameIdx >= contextNames.size()) {
			contextNameIdx[nameIdx] = e.context_id;
			contextNames.append(index.contextName(e.context_id));
		}

		QVariantMap item;
		item["title"] = e.title;
		item["row"] = nameIdx;
		item["ctx"] = e.context_id;
		item["color"] = QColor::fromRgb(e.color);
		item["icon"] = typeIcons[e.type];
		item["start"] = e.start;
		item["end"] = e.end;

		items.append(item);
	}

	// Get snapshot positions
	QVariantList snapshots;
	for(const SnapshotEntry &e : index.snapshots())
		snapshots.append(e.pos);

	QVariantMap indexmap;
	indexmap["names"] = contextNames;
	indexmap["items"] = items;
	indexmap["snapshots"] = snapshots;
	return indexmap;
}

void PlaybackController::setIndexItemSilenced(int idx, bool silence)
{
	if(!m_indexloader) {
		qWarning("setIndexItemSilenced: index not loaded!");
		return;
	}

	m_indexloader->index().setSilenced(idx, silence);
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

	emit canSaveFrameChanged();
}

void PlaybackController::exporterError(const QString &message)
{
	// Stop playback on error
	if(isPlaying())
		setPlaying(false);

	// Show warning message
	// TODO move this away from here
	QMessageBox::warning(0, tr("Video error"), message);

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
	Q_ASSERT(count>0);
	if(m_exporter) {
		QImage img = m_canvas->toImage();
		if(!img.isNull()) {
			Q_ASSERT(m_exporterReady);
			m_exporterReady = false;
			emit canSaveFrameChanged();
			m_exporter->saveFrame(img, count);
		}
	}
}

void PlaybackController::stopExporter()
{
	Q_ASSERT(m_exporter);
	if(isPlaying())
		setPlaying(false);
	m_exporter->finish();
}

bool PlaybackController::waitForExporter()
{
	return m_exporter && !m_exporterReady;
}

}
