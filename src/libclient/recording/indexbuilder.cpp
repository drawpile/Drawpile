/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2019 Calle Laakkonen

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

#include "indexbuilder.h"
#include "index_p.h"

#include "../libshared/record/reader.h"
#include "../libshared/net/undo.h"
#include "../libshared/net/recording.h"

#include "canvas/statetracker.h"
#include "canvas/layerlist.h"

#include "core/layerstack.h"
#include "core/layer.h"
#include "core/annotationmodel.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QSaveFile>

namespace recording {

IndexBuilder::IndexBuilder(const QString &inputfile, const QString &targetfile, const QByteArray &hash, QObject *parent)
	: QObject(parent), m_inputfile(inputfile), m_targetfile(targetfile), m_recordingHash(hash)
{
}

void IndexBuilder::abort()
{
	m_abortflag = 1;
	qDebug() << "aborting indexing...";
}

void IndexBuilder::run()
{
	// Open the recording
	Reader reader(m_inputfile);

	Compatibility readerOk = reader.open();
	if(readerOk != COMPATIBLE && readerOk != MINOR_INCOMPATIBILITY) {
		qWarning() << "Couldn't open recording for indexing. Error code" << readerOk;
		emit done(false, reader.errorString());
		return;
	}

	// Open output file
	QSaveFile outfile(m_targetfile);
	if(!outfile.open(QIODevice::WriteOnly)) {
		emit done(false, outfile.errorString());
		return;
	}

	QDataStream stream(&outfile);

	// Write header
	stream.writeRawData("DPIDX", 5);
	stream << INDEX_VERSION;
	stream << m_recordingHash;
	const auto indexOffsetPos = stream.device()->pos();
	stream << quint32(0); // placeholder (index entry vector offset)
	stream << quint32(0); // placeholder (number of messages in recording)

	// Generate index and write snapshots and thumbnails
	if(!generateIndex(stream, reader)) {
		emit done(false, QString());
		return;
	}

	// Fill in the placeholders
	const auto indexOffset = stream.device()->pos();
	stream.device()->seek(indexOffsetPos);
	stream << quint32(indexOffset);
	stream << quint32(m_messageCount);
	stream.device()->seek(indexOffset);

	// Write the index
	for(const IndexEntry &entry : m_index)
		stream << entry;

	// Done!
	if(!outfile.commit()) {
		emit done(false, outfile.errorString());

	} else {
		emit done(true, QString());
	}
}

namespace {

// Tile --> index file offset mapping
typedef QHash<paintcore::Tile, quint32> IndexedTiles;

static quint32 writeTile(QDataStream &stream, const IndexedTiles &oldTileMap, IndexedTiles &newTileMap, const paintcore::Tile &tile)
{
	quint32 tileOffset;
	if(tile.isNull()) {
		tileOffset = 0;

	} else if(newTileMap.contains(tile)) {
		tileOffset = newTileMap[tile];

	} else if(oldTileMap.contains(tile)) {
		tileOffset = oldTileMap[tile];
		newTileMap[tile] = tileOffset;

	} else {
		tileOffset = quint32(stream.device()->pos());
		stream << tile;
		newTileMap[tile] = tileOffset;
	}

	return tileOffset;
}

static quint32 writeLayer(QDataStream &stream, const paintcore::Layer *layer, const IndexedTiles &oldTileMap, IndexedTiles &newTileMap, bool writeSublayers=true)
{
	IndexedLayer indexedLayer {
		QVector<quint32>(),
		QVector<quint32>(),
		layer->info()
	};

	if(writeSublayers) {
		for(const paintcore::Layer *sublayer : layer->sublayers()) {
			if(sublayer->id() > 0) {
				indexedLayer.sublayerOffsets << writeLayer(stream, sublayer, oldTileMap, newTileMap, false);
			}
		}
	}

	indexedLayer.tileOffsets.reserve(layer->tiles().size());
	for(const paintcore::Tile &tile : layer->tiles()) {
		indexedLayer.tileOffsets << writeTile(stream, oldTileMap, newTileMap, tile);
	}

	// Dependencies written, write the actual layer now
	const quint32 layerOffset = quint32(stream.device()->pos());
	stream << indexedLayer;

	return layerOffset;
}

static quint32 writeAnnotation(QDataStream &stream, const paintcore::Annotation &annotation)
{
	const auto pos = quint32(stream.device()->pos());
	annotation.toDataStream(stream);
	return pos;
}

struct LayerStackWriteResult {
	IndexedTiles tileMap;
	quint32 offset;
};

static LayerStackWriteResult writeLayerStack(QDataStream &stream, const paintcore::Savepoint &savepoint, const IndexedTiles &oldTileMap)
{
	LayerStackWriteResult result;
	IndexedLayerStack indexedStack;

	indexedStack.backgroundTileOffset = writeTile(stream, oldTileMap, result.tileMap, savepoint.background);

	for(const paintcore::Layer *layer : savepoint.layers) {
		// TODO deduplicate?
		indexedStack.layerOffsets << writeLayer(stream, layer, oldTileMap, result.tileMap);
	}

	for(const paintcore::Annotation &annotation : savepoint.annotations) {
		// TODO deduplicate
		indexedStack.annotationOffsets << writeAnnotation(stream, annotation);
	}

	indexedStack.size = savepoint.size;

	result.offset = quint32(stream.device()->pos());
	stream << indexedStack;

	return result;
}

} // end anonymous namespace

bool IndexBuilder::generateIndex(QDataStream &stream, Reader &reader)
{
	static const qint64 SNAPSHOT_INTERVAL_MS = 500; // snapshot interval in milliseconds
	static const int SNAPSHOT_MIN_ACTIONS = 25;     // minimum number of actions between snapshots
	static const int THUMBNAIL_INTERVAL = 1000;     // minimum number of actions between thumbnails

	paintcore::LayerStack image;
	canvas::LayerListModel layermodel;
	canvas::StateTracker statetracker(&image, &layermodel, 1);

	MessageRecord record;
	QElapsedTimer timer;
	int messagesSinceLastEntry = SNAPSHOT_MIN_ACTIONS + 1;
	int messagesSinceLastThumbnail = THUMBNAIL_INTERVAL + 1;
	LayerStackWriteResult lastSnapshot;

	do {
		if(m_abortflag.load()) {
			qWarning() << "Indexing aborted";
			return false;
		}

		const qint64 messageOffset = reader.filePosition();
		record = reader.readNext();
		if(record.status == MessageRecord::OK) {
			if(record.message->isCommand())
				statetracker.receiveCommand(protocol::MessagePtr::fromNullable(record.message));

			if(
				record.message->type() == protocol::MSG_MARKER ||
				(
					messagesSinceLastEntry > SNAPSHOT_MIN_ACTIONS &&
					(!timer.isValid() || timer.hasExpired(SNAPSHOT_INTERVAL_MS))
				)
			) {
				messagesSinceLastEntry = 0;

				// A snapshot is saved at each index entry
				const canvas::StateSavepoint sp = statetracker.createSavepoint(0);
				lastSnapshot = writeLayerStack(stream, sp.canvas(), lastSnapshot.tileMap);

				// A thumbnail is saved no more often than once every THUMBNAIL_INTERVAL messages
				QImage thumbnail;
				if(messagesSinceLastThumbnail >= THUMBNAIL_INTERVAL) {
					messagesSinceLastThumbnail = 0;
					thumbnail = sp.thumbnail(QSize(171, 128));
				}

				// Remember the position of the message and the state snapshot at that point in time
				m_index << IndexEntry {
					quint32(reader.currentIndex()),
					messageOffset,
					lastSnapshot.offset,
					record.message->type() == protocol::MSG_MARKER ?
						record.message.cast<protocol::Marker>().text()
						:
						QString(),
					thumbnail
				};

				emit progress(messageOffset);

				timer.start();
			}

			++messagesSinceLastEntry;
			++messagesSinceLastThumbnail;

		} else if(record.status == MessageRecord::INVALID) {
			qWarning() << "invalid message type" << record.invalid_type << "at index" << reader.currentIndex() << " and offset" << messageOffset;
		}
	} while(record.status != MessageRecord::END_OF_RECORDING);

	m_messageCount = reader.currentIndex();

	return true;
}

}

