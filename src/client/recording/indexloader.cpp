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

#include "index.h"
#include "index_p.h"
#include "indexloader.h"

#include "canvas/statetracker.h"
#include "core/annotationmodel.h"
#include "core/layerstack.h"
#include "core/layer.h"

#include <QFile>
#include <QImage>
#include <QCache>

namespace recording {

struct IndexLoader::Private : public QSharedData
{
	int messageCount;

	QString recordingfile;
	QByteArray recordingHash;
	QFile file;
	QDataStream stream;

	QVector<IndexEntry> index;
	QVector<IndexEntry> markers;
	QVector<IndexEntry> thumbnails;

	QCache<quint32, paintcore::Tile> tileCache;

	paintcore::Tile readTile(quint32 offset);
	paintcore::Layer *readLayer(quint32 layerOffset, const QSize &size, bool readSublayers=true);
};

IndexLoader::IndexLoader()
{
}

IndexLoader::IndexLoader(const QString &recording, const QString &index, const QByteArray &recordingHash)
	: d(new Private)
{
	d->recordingfile = recording;
	d->recordingHash = recordingHash;
	d->file.setFileName(index);
	d->messageCount = 0;
	d->tileCache.setMaxCost(2000);
}

IndexLoader::IndexLoader(const IndexLoader &other)
	: d(other.d)
{
}

IndexLoader::~IndexLoader()
{
}

IndexLoader &IndexLoader::operator=(const IndexLoader &other)
{
	d = other.d;
	return *this;
}

QVector<IndexEntry> IndexLoader::index() const { return d->index; }
QVector<IndexEntry> IndexLoader::markers() const { return d->markers; }
QVector<IndexEntry> IndexLoader::thumbnails() const { return d->thumbnails; }
int IndexLoader::messageCount() const { return d->messageCount; }

bool IndexLoader::open()
{	
	if(!d)
		return false;

	if(!d->file.open(QIODevice::ReadOnly))
		return false;

	d->stream.setDevice(&d->file);

	// Check magic numbers
	char magic[5];
	d->stream.readRawData(magic, 5);
	if(memcmp(magic, "DPIDX", 5) != 0) {
		qWarning("%s: not an index file", qPrintable(d->file.fileName()));
		return false;
	}

	quint32 formatVersion;
	d->stream >> formatVersion;

	if(formatVersion != INDEX_VERSION) {
		qWarning("%s: wrong version (%d)", qPrintable(d->file.fileName()), formatVersion);
		return false;
	}

	QByteArray checksum;
	d->stream >> checksum;

	if(checksum != d->recordingHash) {
		qWarning("%s: checksum mismatch", qPrintable(d->file.fileName()));
		return false;
	}

	// Read header fields
	quint32 entryVectorOffset;
	d->stream >> entryVectorOffset >> d->messageCount;

	// Load index entry vector
	d->stream.device()->seek(entryVectorOffset);
	while(d->stream.status() == QDataStream::Ok) {
		IndexEntry e;
		d->stream
			>> e.index
			>> e.messageOffset
			>> e.snapshotOffset
			>> e.title
			>> e.thumbnail;

		if(d->stream.status() == QDataStream::ReadPastEnd)
			return true;

		d->index << e;
		if(!e.thumbnail.isNull())
			d->thumbnails << e;
		if(!e.title.isEmpty())
			d->markers << e;
	}

	qWarning("Index reading error!");
	return false;
}

canvas::StateSavepoint IndexLoader::loadSavepoint(const IndexEntry &entry)
{
	d->file.seek(entry.snapshotOffset);
	d->stream.resetStatus();

	// Read layer stack
	IndexedLayerStack layerstack;
	d->stream >> layerstack;

	if(d->stream.status() != QDataStream::Ok) {
		qWarning("Index read error!");
		return canvas::StateSavepoint();
	}

	// Read layers
	QList<paintcore::Layer*> layers;
	for(const quint32 layerOffset : layerstack.layerOffsets) {
		auto *layer = d->readLayer(layerOffset, layerstack.size);
		if(!layer) {
			for(auto *l : layers)
				delete l;
			return canvas::StateSavepoint();
		}
		layers << layer;
	}


	// Read annotations
	QList<paintcore::Annotation> annotations;
	for(const quint32 annotationOffset : layerstack.annotationOffsets) {
		d->file.seek(annotationOffset);
		annotations << paintcore::Annotation::fromDataStream(d->stream);
	}

	// Make savepoint
	paintcore::Savepoint sp;
	sp.layers = layers;
	sp.annotations = annotations;
	sp.background = d->readTile(layerstack.backgroundTileOffset);
	sp.size = layerstack.size;

	return canvas::StateSavepoint::fromCanvasSavepoint(sp);
}

paintcore::Tile IndexLoader::Private::readTile(quint32 offset)
{
	if(offset == 0)
		return paintcore::Tile();

	if(tileCache.contains(offset))
		return *tileCache[offset];

	file.seek(offset);
	auto *t = new paintcore::Tile;
	stream >> *t;
	tileCache.insert(offset, t);
	return *t;
}

paintcore::Layer *IndexLoader::Private::readLayer(quint32 layerOffset, const QSize &size, bool readSublayers)
{
	file.seek(layerOffset);

	IndexedLayer il;
	stream >> il;

	if(stream.status() != QDataStream::Ok) {
		qWarning("Could not read layer from index");
		return nullptr;
	}

	QVector<paintcore::Tile> tiles;
	tiles.reserve(il.tileOffsets.size());
	for(const quint32 tileOffset : il.tileOffsets) {
		tiles << readTile(tileOffset);
	}

	QList<paintcore::Layer*> sublayers;
	if(readSublayers) {
		sublayers.reserve(il.sublayerOffsets.size());
		for(const quint32 sublayerOffset : il.sublayerOffsets) {
			sublayers << readLayer(sublayerOffset, size, false);
		}
	}

	return new paintcore::Layer(tiles, size, il.info, sublayers);
}

}
