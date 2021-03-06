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

#ifndef DP_CORE_TILEVECTOR_H
#define DP_CORE_TILEVECTOR_H

#include "tile.h"
#include "../libshared/net/message.h"

#include <QVector>
#include <QColor>

class QSize;
class QPoint;

namespace paintcore {

class Layer;

/**
 * @brief One or more tile to be placed on a layer
 */
struct TileRun {
	Tile tile;
	int col;
	int row;
	int len;      // the length of the tile run (always at least 1)
	QColor color; // if valid, this tile is filled with solid color
};

/**
 * @brief An RLE compressed representation of a layer's tiles
 */
struct LayerTileSet {
	QVector<TileRun> tiles;
	QColor background;

	/**
	 * @brief Generate a compressed tile set representation of a layer
	 *
	 * Using the PutTile command, this is used to make an efficient
	 * layer initialization command sequence.
	 */
	static LayerTileSet fromLayer(const Layer &layer);
	static LayerTileSet fromImage(const QImage &image);
	static LayerTileSet fromImage(const QImage &image, const QSize &layerSize, const QPoint &offset);

	/**
	 * @brief Generate PutTiles commands
	 * @param contextid the message context ID to use
	 * @param layerId target layer ID
	 * @param sublayer target sublayer (0 for normal layers)
	 * @param msgs where to put the messages
	 */
	void toPutTiles(uint8_t contextid, uint16_t layerId, uint8_t sublayer, protocol::MessageList &msgs) const;
};

}

#endif
