/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#include "tilevector.h"
#include "layer.h"
#include "../shared/net/layer.h"
#include "../shared/net/image.h"

#include <QImage>

namespace paintcore {

LayerTileSet LayerTileSet::fromLayer(const Layer &layer)
{
	const QVector<Tile> tiles = layer.tiles();
	const int cols = Tile::roundTiles(layer.width());

	Q_ASSERT(!tiles.isEmpty());

	QVector<TileRun> runs;

	// First, Run Length Encode the tile vector
	runs << TileRun { tiles.first(), 0, 0, 1, tiles.first().solidColor() };

	for(int i=1;i<tiles.size();++i) {
		if(runs.last().len < 0xffff && runs.last().tile.equals(tiles.at(i))) {
			runs.last().len++;
		} else {
			runs << TileRun { tiles.at(i), i%cols, i/cols, 1, tiles.at(i).solidColor() };
		}
	}

	// Count the number of solid color tiles.
	QHash<quint32, int> colors;
	for(const TileRun &tr : runs) {
		if(tr.color.isValid())
			colors[tr.color.rgba()] += tr.len;
	}

	// If half of the tiles are of the same
	// solid color, use that as the background color.
	// Otherwise, transparency is a safe default choice.
	QColor background = Qt::transparent;
	const int treshold = tiles.length() / 2;
	for(QHash<quint32, int>::const_iterator i = colors.constBegin();i!=colors.constEnd();++i) {
		if(i.value() >= treshold) {
			background = QColor::fromRgba(i.key());
			break;
		}
	}

	// Remove solid color tiles that match the background
	QMutableVectorIterator<TileRun> tri(runs);
	while(tri.hasNext()) {
		if(tri.next().color == background)
			tri.remove();
	}

	// All done!
	return LayerTileSet {
		runs,
		background
	};
}

LayerTileSet LayerTileSet::fromImage(const QImage &image)
{
	Layer l(0, QString(), Qt::transparent, image.size());
	EditableLayer(&l, nullptr).putImage(0, 0, image, paintcore::BlendMode::MODE_REPLACE);
	return fromLayer(l);
}

QList<protocol::MessagePtr> LayerTileSet::toInitCommands(int contextId, int layerId, const QString &layerTitle)
{
	QList<protocol::MessagePtr> msgs;

	msgs << protocol::MessagePtr(new protocol::LayerCreate(
		contextId,
		layerId,
		0,
		background.rgba(),
		0,
		layerTitle
	));

	for(const TileRun &t : tiles) {
		Q_ASSERT(t.len>0);
		if(t.color.isValid()) {
			msgs << protocol::MessagePtr(new protocol::PutTile(contextId, layerId, 0, t.col, t.row, t.len-1, t.color.rgba()));
		} else {
			Q_ASSERT(!t.tile.isNull());
			msgs << protocol::MessagePtr(new protocol::PutTile(contextId, layerId, 0, t.col, t.row, t.len-1,
				qCompress(reinterpret_cast<const uchar*>(t.tile.constData()), paintcore::Tile::BYTES)
				));
		}
	}

	return msgs;
}

}

