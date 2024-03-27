// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/tile.h>
#include <dpengine/tile_iterator.h>
}
#include "libclient/canvas/tilecache.h"
#include <QPainter>
#include <QtGlobal>

namespace canvas {

TileCache::~TileCache()
{
	DP_free(m_pixels);
}

void TileCache::clear()
{
	m_dirtyTiles.clear();
	m_dirtyNavigatorTiles.clear();
	m_needsDirtyCheck = false;
	m_needsNavigatorDirtyCheck = false;
	m_resized = true;
	m_width = 0;
	m_height = 0;
	m_lastWidth = 0;
	m_lastHeight = 0;
	m_lastTileX = -1;
	m_lastTileY = -1;
	m_xtiles = 0;
	m_ytiles = 0;
	m_offsetX = 0;
	m_offsetY = 0;
}

void TileCache::resize(int width, int height, int offsetX, int offsetY)
{
	Q_ASSERT(width >= 0);
	Q_ASSERT(height >= 0);
	DP_TileCounts tc = DP_tile_counts_round(width, height);
	int tileTotal = tc.x * tc.y;
	size_t requiredCapacity =
		size_t(tileTotal) * DP_TILE_LENGTH * sizeof(*m_pixels);
	if(m_capacity == requiredCapacity) {
		memset(m_pixels, 0, requiredCapacity);
	} else {
		DP_free(m_pixels);
		m_pixels = static_cast<DP_Pixel8 *>(DP_malloc_zeroed(requiredCapacity));
		m_capacity = requiredCapacity;
	}

	m_dirtyTiles.fill(true, tileTotal);
	m_dirtyNavigatorTiles.fill(true, tileTotal);
	m_needsDirtyCheck = true;
	m_needsNavigatorDirtyCheck = true;
	m_resized = true;
	m_width = width;
	m_height = height;
	m_lastWidth = width % DP_TILE_SIZE;
	m_lastHeight = height % DP_TILE_SIZE;
	m_lastTileX = m_lastWidth == 0 ? tc.x : tc.x - 1;
	m_lastTileY = m_lastHeight == 0 ? tc.y : tc.y - 1;
	m_xtiles = tc.x;
	m_ytiles = tc.y;
	m_offsetX += offsetX;
	m_offsetY += offsetY;
}

TileCache::RenderResult
TileCache::render(int tileX, int tileY, const DP_Pixel8 *src)
{
	bool plainX = tileX < m_lastTileX;
	bool plainY = tileY < m_lastTileY;
	int i = tileIndex(tileX, tileY);
	DP_Pixel8 *dst = pixelsAt(i);
	if(plainX && plainY) {
		memcpy(dst, src, DP_TILE_LENGTH * sizeof(*dst));
	} else {
		int w = plainX ? DP_TILE_SIZE : m_lastWidth;
		int h = plainY ? DP_TILE_SIZE : m_lastHeight;
		size_t row_size = size_t(w) * sizeof(*dst);
		for(int y = 0; y < h; ++y) {
			memcpy(dst + y * w, src + y * DP_TILE_SIZE, row_size);
		}
	}

	RenderResult result;
	bool &dirtyTile = m_dirtyTiles[i];
	if(!dirtyTile) {
		dirtyTile = true;
		if(!m_needsDirtyCheck) {
			m_needsDirtyCheck = true;
			result.dirtyCheck = true;
		}
	}

	bool &dirtyNavigatorTile = m_dirtyNavigatorTiles[i];
	if(!dirtyNavigatorTile) {
		dirtyNavigatorTile = true;
		if(!m_needsNavigatorDirtyCheck) {
			m_needsNavigatorDirtyCheck = true;
			result.navigatorDirtyCheck = true;
		}
	}
	return result;
}

QImage TileCache::toImage() const
{
	QImage img(m_width, m_height, QImage::Format_ARGB32_Premultiplied);
	QPainter painter;
	painter.begin(&img);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	for(int tileY = 0; tileY < m_ytiles; ++tileY) {
		int tileH = tileY < m_lastTileY ? DP_TILE_SIZE : m_lastHeight;
		for(int tileX = 0; tileX < m_xtiles; ++tileX) {
			int tileW = tileX < m_lastTileX ? DP_TILE_SIZE : m_lastWidth;
			painter.drawImage(
				QRect(tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE, tileW, tileH),
				QImage(
					reinterpret_cast<const uchar *>(
						pixelsAt(tileIndex(tileX, tileY))),
					tileW, tileH, QImage::Format_ARGB32_Premultiplied));
		}
	}
	painter.end();
	return img;
}

QImage TileCache::toSubImage(const QRect &rect)
{
	QRect r = rect.intersected(QRect(0, 0, m_width, m_height));
	if(r.isEmpty()) {
		return QImage();
	} else {
		int x = r.x();
		int y = r.y();
		int w = r.width();
		int h = r.height();
		QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
		uint32_t *imgPixels = reinterpret_cast<uint32_t *>(img.bits());
		DP_TileIterator ti =
			DP_tile_iterator_make(m_width, m_height, DP_rect_make(x, y, w, h));
		while(DP_tile_iterator_next(&ti)) {
			DP_TileIntoDstIterator tid = DP_tile_into_dst_iterator_make(&ti);
			const DP_Pixel8 *tilePixels = pixelsAt(tileIndex(ti.col, ti.row));
			int tileWidth = ti.col < m_lastTileX ? DP_TILE_SIZE : m_lastWidth;
			while(DP_tile_into_dst_iterator_next(&tid)) {
				imgPixels[tid.dst_y * w + tid.dst_x] =
					tilePixels[tid.tile_y * tileWidth + tid.tile_x].color;
			}
		}
		return img;
	}
}

bool TileCache::getResizeReset(Resize &outResize)
{
	if(m_resized) {
		outResize.width = m_width;
		outResize.height = m_height;
		outResize.offsetX = m_offsetX;
		outResize.offsetY = m_offsetY;
		m_resized = false;
		m_offsetX = 0;
		m_offsetY = 0;
		return true;
	} else {
		return false;
	}
}

void TileCache::eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn)
{
	int right = qMin(tileArea.right(), m_xtiles - 1);
	int bottom = qMin(tileArea.bottom(), m_ytiles - 1);
	for(int tileY = tileArea.top(); tileY <= bottom; ++tileY) {
		for(int tileX = tileArea.left(); tileX <= right; ++tileX) {
			int i = tileIndex(tileX, tileY);
			bool &dirtyTile = m_dirtyTiles[i];
			if(dirtyTile) {
				dirtyTile = false;
				fn(rectAt(tileX, tileY), pixelsAt(i));
			}
		}
	}
	m_needsDirtyCheck = false;
}

void TileCache::eachDirtyNavigatorTileReset(bool all, const OnTileFn &fn)
{
	for(int tileY = 0; tileY < m_ytiles; ++tileY) {
		for(int tileX = 0; tileX < m_xtiles; ++tileX) {
			int i = tileIndex(tileX, tileY);
			bool &dirtyNavigatorTile = m_dirtyNavigatorTiles[i];
			if(all || dirtyNavigatorTile) {
				dirtyNavigatorTile = false;
				fn(rectAt(tileX, tileY), pixelsAt(i));
			}
		}
	}
	m_needsNavigatorDirtyCheck = false;
}

QRect TileCache::rectAt(int tileX, int tileY) const
{
	return QRect(
		tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE,
		tileX < m_lastTileX ? DP_TILE_SIZE : m_lastWidth,
		tileY < m_lastTileY ? DP_TILE_SIZE : m_lastHeight);
}

}
