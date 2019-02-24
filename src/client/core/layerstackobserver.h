/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef LAYERSTACKOBSERVER_H
#define LAYERSTACKOBSERVER_H

#include "tile.h"

#include <QBitArray>
#include <QRect>

class QPaintDevice;

namespace paintcore {

class LayerStack;

/**
 * @brief An abstract base class for objects that react to changes in a layer stack
 */
class LayerStackObserver
{
public:
	LayerStackObserver();
	virtual ~LayerStackObserver();

	//! Start observing a layer stack
	void attachToLayerStack(LayerStack *layerstack);

	//! Detach from the layer stack currently being observed (if any)
	void detachFromLayerStack();

	//! Get the layer stack under observation
	LayerStack *layerStack() { return m_layerstack; }
	const LayerStack *layerStack() const { return m_layerstack; }

	//! Canvas background tile change
	void canvasBackgroundChanged(const Tile &tile);

	//! Mark the tiles under the area dirty
	void markDirty(const QRect &area);

	//! Mark entire canvas as dirty
	void markDirty();

	//! Mark the tile at the given index as dirty
	void markDirty(int x, int y);

	//! Mark the tile at the given index as dirty
	void markDirty(int index);

	/**
	 * @brief A sequence of canvas alterations just completed
	 *
	 * The areaChanged protected virtual function will be called.
	 */
	void canvasWriteSequenceDone();

	/**
	 * @brief The canvas was just resized
	 * @param xoffset
	 * @param yoffset
	 * @param oldsize
	 */
	void canvasResized(int xoffset, int yoffset, const QSize &oldsize);

protected:
	//! An editing operation just finished and pixels under the given area have changed
	virtual void areaChanged(const QRect &area) = 0;

	//! Canvas size just changed
	virtual void resized(int xoffset, int yoffset, const QSize &oldsize) = 0;

	/**
	 * @brief Paint all changed tiles in the given region onto the target paint device
	 *
	 * The dirty flag will be cleared for each painted tile.
	 *
	 * @param rect
	 * @param target
	 */
	void paintChangedTiles(const QRect &rect, QPaintDevice *target);

private:
	LayerStack *m_layerstack;
	Tile m_paintBackgroundTile;

	QBitArray m_dirtytiles;
	QRect m_dirtyrect;
};

}

#endif
