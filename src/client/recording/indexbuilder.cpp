/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2016 Calle Laakkonen

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

#include "recording/indexbuilder.h"
#include "../shared/record/reader.h"
#include "../shared/net/undo.h"
#include "../shared/net/recording.h"

#include "canvas/statetracker.h"
#include "core/layerstack.h"
#include "canvas/layerlist.h"

#include <QDebug>
#include <QBuffer>
#include <QBuffer>
#include <QElapsedTimer>
#include <KZip>

namespace recording {

IndexBuilder::IndexBuilder(const QString &inputfile, const QString &targetfile, QObject *parent)
	: QObject(parent), m_inputfile(inputfile), m_targetfile(targetfile)
{
}

void IndexBuilder::abort()
{
	m_abortflag = 1;
	qDebug() << "aborting indexing...";
}

void IndexBuilder::run()
{
	// Open output file
	KZip zip(m_targetfile);
	if(!zip.open(QIODevice::WriteOnly)) {
		emit done(false, tr("Error opening %1 for writing").arg(m_targetfile));
		return;
	}

	zip.setCompression(KZip::NoCompression);

	// Write recording hash
	QByteArray hash = hashRecording(m_inputfile);
	zip.writeFile("hash", hash);

	// Open the recording
	Reader reader(m_inputfile);

	Compatibility readerOk = reader.open();
	if(readerOk != COMPATIBLE && readerOk != MINOR_INCOMPATIBILITY) {
		qWarning() << "Couldn't open recording for indexing. Error code" << readerOk;
		emit done(false, reader.errorString());
		return;
	}

	// Generate index and write snapshots and thumbnails
	generateIndex(zip, reader);

	// Write the index
	QBuffer indexBuffer;
	indexBuffer.open(QBuffer::ReadWrite);
	m_index.writeIndex(&indexBuffer);

	zip.setCompression(KZip::DeflateCompression);
	zip.writeFile("index", indexBuffer.data());

	if(!zip.close()) {
		emit done(false, tr("Error writing file"));
		return;
	}

	emit done(true, QString());
}

void IndexBuilder::generateIndex(KZip &zip, Reader &reader)
{
	static const qint64 SNAPSHOT_INTERVAL_MS = 1000; // snapshot interval in milliseconds
	static const int SNAPSHOT_MIN_STOPS = 25; // minimum number of stops between snapshots
	static const int THUMBNAIL_INTERVAL = 1000; // minimum number of actions between thumbnails

	// We must replay the recorded session to generate canvas snapshots
	paintcore::LayerStack image;
	canvas::LayerListModel layermodel;
	canvas::StateTracker statetracker(&image, &layermodel, 1);

	// Generate index and snapshots
	MessageRecord record;
	QElapsedTimer timer;
	int snapshotStops = 0;
	int thumbnailActions = 0;
	int snapshotCount = 0;
	int thumbnailCount = 0;
	timer.start();
	do {
		if(m_abortflag.load()) {
			qWarning() << "Indexing aborted";
			emit done(false, "aborted");
			return;
		}

		qint64 offset = reader.filePosition();
		record = reader.readNext();
		if(record.status == MessageRecord::OK) {
			protocol::MessagePtr msg(record.message);

			if(msg->isCommand())
				statetracker.receiveCommand(msg);

			// Add a stop for each UndoPoint and Marker
			if(msg->type() == protocol::MSG_UNDOPOINT || msg->type() == protocol::MSG_MARKER) {
				StopEntry stop { quint32(reader.currentIndex()), offset, 0 };
				++snapshotStops;

				// Snapshots can be saved at stops, but with some limits:
				// - Must have at least SNAPSHOT_MIN_ACTIONS actions between them
				// - Must be separated by at least SNAPSHOT_INTERVAL_MS milliseconds of CPU time.
				if(snapshotCount==0 ||
						(
						 	(timer.hasExpired(SNAPSHOT_INTERVAL_MS) || msg->type() == protocol::MSG_MARKER)
							&& snapshotStops>=SNAPSHOT_MIN_STOPS
						)
						) {
					++snapshotCount;
					emit progress(offset);
					canvas::StateSavepoint sp = statetracker.createSavepoint(-1);

					QBuffer buf;
					buf.open(QBuffer::ReadWrite);
					{
						QDataStream ds(&buf);
						sp.toDatastream(ds);
					}

					zip.setCompression(KZip::DeflateCompression);
					zip.writeFile(QString("snapshot/%1").arg(m_index.size()), buf.data());
					stop.flags |= StopEntry::HAS_SNAPSHOT;

					snapshotStops= 0;
					timer.restart();
				}

				m_index.m_stops.append(stop);

				// Add marker entry if message was a MARKER
				if(msg->type() == protocol::MSG_MARKER) {
					m_index.m_markers.append(MarkerEntry {
						quint32(m_index.size())-1,
						msg.cast<protocol::Marker>().text()
						});
				}
			}

			// Add thumbnails at even intervals
			if(thumbnailCount==0 || thumbnailActions >= THUMBNAIL_INTERVAL) {
				thumbnailActions = 0;

				QImage thumb = image.toFlatImage(false).scaled(171, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
				QBuffer buf;
				buf.open(QBuffer::ReadWrite);
				thumb.save(&buf, "PNG");
				zip.setCompression(KZip::NoCompression);
				zip.writeFile(QString("thumbnail/%1").arg(thumbnailCount), buf.data());
				++thumbnailCount;
			} else {
				++thumbnailActions;
			}

		} else if(record.status == MessageRecord::INVALID) {
			qWarning() << "invalid message type" << record.error.type << "at index" << reader.currentIndex() << " and offset" << offset;
		}
	} while(record.status != MessageRecord::END_OF_RECORDING);

	m_index.m_actioncount = reader.currentIndex();
}

}

