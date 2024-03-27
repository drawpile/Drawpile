// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TILECACHE_H
#define LIBCLIENT_CANVAS_TILECACHE_H
extern "C" {
#include <dpengine/pixels.h>
}
#include <QImage>
#include <QRect>
#include <QSize>
#include <QVector>
#include <functional>

class QPixmap;

namespace canvas {

// Holds rendered tiles in a format suitable for glTexSubImage2D and records
// changed tiles since the last render, both for the canvas itself and for the
// navigator. The pixels array contains each tile in sequence. Tiles at the
// right and bottom edge may have smaller dimensions, but they still allocate a
// full tile of space to avoid complicating the indexing.
class TileCache final {
	using OnTileFn = std::function<void(const QRect &rect, const void *pixels)>;

public:
	struct RenderResult {
		bool dirtyCheck = false;
		bool navigatorDirtyCheck = false;
	};

	struct Resize {
		int width;
		int height;
		int offsetX;
		int offsetY;
	};

	TileCache() {}
	~TileCache();

	TileCache(const TileCache &) = delete;
	TileCache(TileCache &&) = delete;
	TileCache &operator=(const TileCache &) = delete;
	TileCache &operator=(TileCache &&) = delete;

	QSize size() const { return QSize(m_width, m_height); }

	void clear();
	void resize(int width, int height, int offsetX, int offsetY);

	RenderResult render(int tileX, int tileY, const DP_Pixel8 *src);

	QImage toImage() const;
	QImage toSubImage(const QRect &rect);

	bool getResizeReset(Resize &outResize);
	void eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn);
	void eachDirtyNavigatorTileReset(bool all, const OnTileFn &fn);

private:
	DP_Pixel8 *pixelsAt(int i) const { return m_pixels + i * DP_TILE_LENGTH; }

	QRect rectAt(int tileX, int tileY) const;

	int tileIndex(int tileX, int tileY) const
	{
		return tileY * m_xtiles + tileX;
	}

	DP_Pixel8 *m_pixels = nullptr;
	size_t m_capacity = 0;
	QVector<bool> m_dirtyTiles;
	QVector<bool> m_dirtyNavigatorTiles;
	bool m_needsDirtyCheck = false;
	bool m_needsNavigatorDirtyCheck = false;
	bool m_resized = false;
	int m_width = 0;
	int m_height = 0;
	int m_lastWidth = 0;
	int m_lastHeight = 0;
	int m_lastTileX = -1;
	int m_lastTileY = -1;
	int m_xtiles = 0;
	int m_ytiles = 0;
	int m_offsetX = 0;
	int m_offsetY = 0;
};

}

#endif
