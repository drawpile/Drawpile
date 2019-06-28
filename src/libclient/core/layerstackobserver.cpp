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

#include "layerstackobserver.h"
#include "layerstack.h"
#include "concurrent.h"

#include <QPainter>

namespace paintcore {

LayerStackObserver::LayerStackObserver()
	: m_layerstack(nullptr)
{
}

LayerStackObserver::~LayerStackObserver()
{
	detachFromLayerStack();
}

void LayerStackObserver::attachToLayerStack(LayerStack *layerstack)
{
	Q_ASSERT(layerstack);

	if(m_layerstack)
		m_layerstack->m_observers.removeAll(this);

	m_layerstack = layerstack;
	m_layerstack->m_observers.append(this);

	canvasResized(0, 0, QSize());
	canvasBackgroundChanged(layerstack->background());
}

void LayerStackObserver::detachFromLayerStack()
{
	if(m_layerstack) {
		m_layerstack->m_observers.removeAll(this);
		m_layerstack = nullptr;
	}
}

void LayerStackObserver::markDirty(const QRect &area)
{
	Q_ASSERT(m_layerstack);

	if(m_layerstack->m_layers.isEmpty() || m_layerstack->m_width<=0 || m_layerstack->m_height<=0)
		return;

	const int XT = m_layerstack->m_xtiles;
	const int YT = m_layerstack->m_ytiles;

	Q_ASSERT(XT * YT == m_dirtytiles.size());

	const int tx0 = qBound(0, area.left() / Tile::SIZE, XT-1);
	const int tx1 = qBound(tx0, area.right() / Tile::SIZE, XT-1) + 1;
	int ty0 = qBound(0, area.top() / Tile::SIZE, YT-1);
	const int ty1 = qBound(ty0, area.bottom() / Tile::SIZE, YT-1);

	for(;ty0<=ty1;++ty0) {
		m_dirtytiles.fill(true, ty0*XT + tx0, ty0*XT + tx1);
	}
	m_dirtyrect |= area;
}

void LayerStackObserver::markDirty()
{
	m_dirtytiles.fill(true);
	m_dirtyrect = QRect(QPoint(), m_layerstack->size());
}

void LayerStackObserver::markDirty(int x, int y)
{
	Q_ASSERT(x>=0 && x < m_layerstack->m_xtiles);
	Q_ASSERT(y>=0 && y < m_layerstack->m_ytiles);
	Q_ASSERT(m_layerstack->m_xtiles * m_layerstack->m_ytiles == m_dirtytiles.size());

	m_dirtytiles.setBit(y*m_layerstack->m_xtiles + x);

	m_dirtyrect |= QRect(x*Tile::SIZE, y*Tile::SIZE, Tile::SIZE, Tile::SIZE);
}

void LayerStackObserver::markDirty(int index)
{
	Q_ASSERT(m_layerstack->m_xtiles * m_layerstack->m_ytiles == m_dirtytiles.size());
	Q_ASSERT(index>=0 && index < m_dirtytiles.size());

	m_dirtytiles.setBit(index);

	const int y = index / m_layerstack->m_xtiles;
	const int x = index % m_layerstack->m_xtiles;

	m_dirtyrect |= QRect(x*Tile::SIZE, y*Tile::SIZE, Tile::SIZE, Tile::SIZE);
}

void LayerStackObserver::canvasWriteSequenceDone()
{
	areaChanged(m_dirtyrect);
	m_dirtyrect = QRect();
}

void LayerStackObserver::canvasResized(int xoffset, int yoffset, const QSize &oldsize)
{
	Q_ASSERT(m_layerstack);
	m_dirtytiles = QBitArray(m_layerstack->m_xtiles * m_layerstack->m_ytiles, true);
	m_dirtyrect = QRect(QPoint(), m_layerstack->size());
	resized(xoffset, yoffset, oldsize);
}

void LayerStackObserver::canvasBackgroundChanged(const Tile &tile)
{
	// Check if background tile has any transparent pixels
	bool isTransparent = tile.isNull();
	if(!tile.isNull()) {
		const quint32 *ptr = tile.constData();
		for(int i=0;i<Tile::LENGTH;++i,++ptr) {
			if(qAlpha(*ptr) < 255) {
				isTransparent = true;
				break;
			}
		}
	}

	// If background tile is (at least partially) transparent, composite it with
	// the checkerboard pattern. (TODO: draw background in the view widget)
	if(isTransparent) {
		Tile::fillChecker(m_paintBackgroundTile.data(), QColor(128,128,128), Qt::white);
		m_paintBackgroundTile.merge(tile, 255, BlendMode::MODE_NORMAL);

	} else {
		m_paintBackgroundTile = tile;
	}

	markDirty();
}

namespace {
	struct UpdateTile {
		UpdateTile(int x_, int y_) : x(x_), y(y_) {}

		int x, y;
		quint32 data[Tile::LENGTH];
	};
}

void LayerStackObserver::paintChangedTiles(const QRect &rect, QPaintDevice *target)
{
	Q_ASSERT(m_layerstack);
	if(m_layerstack->width() <=0 || m_layerstack->height() <= 0)
		return;

	// Affected tile range
	const int tx0 = qBound(0, rect.left() / Tile::SIZE, m_layerstack->m_xtiles-1);
	const int tx1 = qBound(tx0, rect.right() / Tile::SIZE, m_layerstack->m_xtiles-1);
	const int ty0 = qBound(0, rect.top() / Tile::SIZE, m_layerstack->m_ytiles-1);
	const int ty1 = qBound(ty0, rect.bottom() / Tile::SIZE, m_layerstack->m_ytiles-1);

	// Gather list of tiles in need of updating
	QList<UpdateTile*> updates;

	for(int ty=ty0;ty<=ty1;++ty) {
		const int y = ty*m_layerstack->m_xtiles;
		for(int tx=tx0;tx<=tx1;++tx) {
			const int i = y+tx;
			if(m_dirtytiles.testBit(i)) {
				updates.append(new UpdateTile(tx, ty));
				m_dirtytiles.clearBit(i);
			}
		}
	}

	if(!updates.isEmpty()) {
		// Flatten tiles
		concurrentForEach<UpdateTile*>(updates, [this](UpdateTile *t) {
			m_paintBackgroundTile.copyTo(t->data);
			m_layerstack->flattenTile(t->data, t->x, t->y);
		});

		// Paint flattened tiles
		QPainter painter(target);
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		while(!updates.isEmpty()) {
			UpdateTile *ut = updates.takeLast();
			painter.drawImage(
				ut->x*Tile::SIZE,
				ut->y*Tile::SIZE,
				QImage(reinterpret_cast<const uchar*>(ut->data),
					Tile::SIZE, Tile::SIZE,
					QImage::Format_ARGB32_Premultiplied
				)
			);
			delete ut;
		}
	}
}

}
