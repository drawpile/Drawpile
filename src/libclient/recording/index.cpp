/*
   Drawpile - a collaborative drawing program.

5  Copyright (C) 2014-2019 Calle Laakkonen

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

#include <QFile>
#include <QCryptographicHash>

namespace recording {

IndexEntry IndexEntry::nearest(const QVector<IndexEntry> &index, int pos)
{
	Q_ASSERT(!index.isEmpty());

	int i=0;
	while(i<index.size() && int(index.at(i).index) < pos)
		++i;
	return index.at(qMax(0, i-1));
}

QDataStream &operator<<(QDataStream &ds, const IndexEntry &ie)
{
	return ds
		<< ie.index
		<< ie.messageOffset
		<< ie.snapshotOffset
		<< ie.title
		<< ie.thumbnail;
}

QDataStream &operator>>(QDataStream &ds, IndexEntry &ie)
{
	return ds
		>> ie.index
		>> ie.messageOffset
		>> ie.snapshotOffset
		>> ie.title
		>> ie.thumbnail;
}

QDataStream &operator<<(QDataStream &ds, const IndexedLayer &i)
{
	return ds
		<< i.tileOffsets
		<< i.sublayerOffsets
		<< i.info.id
		<< i.info.title
		<< i.info.opacity
		<< i.info.hidden
		<< i.info.censored
		<< i.info.fixed
		<< quint8(i.info.blend);
}

QDataStream &operator>>(QDataStream &ds, IndexedLayer &i)
{
	quint8 blend;
	ds
		>> i.tileOffsets
		>> i.sublayerOffsets
		>> i.info.id
		>> i.info.title
		>> i.info.opacity
		>> i.info.hidden
		>> i.info.censored
		>> i.info.fixed
		>> blend;
	i.info.blend = paintcore::BlendMode::Mode(blend);
	return ds;
}

QDataStream &operator<<(QDataStream &ds, const IndexedLayerStack &i)
{
	return ds
		<< i.layerOffsets
		<< i.annotationOffsets
		<< i.backgroundTileOffset
		<< i.size;
}

QDataStream &operator>>(QDataStream &ds, IndexedLayerStack &i)
{
	return ds
		>> i.layerOffsets
		>> i.annotationOffsets
		>> i.backgroundTileOffset
		>> i.size;
}

QByteArray hashRecording(const QString &filename)
{
	QFile file(filename);
	if(!file.open(QFile::ReadOnly))
		return QByteArray();

	QCryptographicHash hash(QCryptographicHash::Sha256);
	hash.addData(&file);

	return hash.result();
}

}

