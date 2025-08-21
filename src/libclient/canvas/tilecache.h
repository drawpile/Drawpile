// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_CANVAS_TILECACHE_H
#define LIBCLIENT_CANVAS_TILECACHE_H
#include <QImage>
#include <QPixmap>
#include <QRect>
#include <QSize>
#include <QVector>
#include <functional>

union DP_Pixel8;

namespace view {
class SoftwareCanvas;
}

namespace canvas {

// Grid of QPixmaps. QPainter can't deal with oversized pixmaps, so we have to
// split them into multiple chunks.
class PixmapGrid final {
public:
	static constexpr int MAX_SIZE = 32000;

	struct Cell {
		QRect rect;
		QPixmap pixmap;
	};

	void clear();
	void resize(int prevWidth, int prevHeight, int width, int height);

	// The given rect must have tile-aligned x and y coordinates as well as a
	// size that's equal to or less than the dimensions of a tile.
	void renderTile(const QRect &rect, const DP_Pixel8 *src);

	QImage toImage(int width, int height) const;
	QImage toSubImage(int width, int height, const QRect &rect);

	const QVector<Cell> &cells() const { return m_cells; }

private:
	QVector<Cell> m_cells;
};

// Holds pixmaps for the QGraphicsView canvas implementation. This is the
// "legacy" format, probably to be removed at some point.
class PixmapCache final {
public:
	QSize size() const { return QSize(m_width, m_height); }

	void clear();
	void resize(int width, int height);

	QRect render(int tileX, int tileY, const DP_Pixel8 *src);

	QImage toImage() const;
	QImage toSubImage(const QRect &rect);

	const QVector<PixmapGrid::Cell> &cells() const { return m_grid.cells(); }

private:
	PixmapGrid m_grid;
	int m_width = 0;
	int m_height = 0;
	int m_lastWidth = 0;
	int m_lastHeight = 0;
	int m_lastTileX = -1;
	int m_lastTileY = -1;
};

// Holds rendered tiles in a format suitable for the canvas implementation and
// records changed tiles since the last render, both for the canvas itself and
// for the navigator. For the OpenGL canvas, the pixels are held in an array
// containing each tile in sequence, which is a format suitable for passing to
// glTexSubImage2D. Tiles at the right and bottom edge may have smaller
// dimensions, but they still allocate a full tile of space to avoid
// complicating the indexing. For the software canvas, it's a PixmapGrid.
class TileCache final {
	// In the OpenGL canvas, pixels is a pointer to an array of DP_Pixel8 to be
	// placed into the texture. In the software canvas, it's a null pointer and
	// softwareCanvasPixmap() is instead used to retrieve the rendered image.
	using OnTileFn = std::function<void(const QRect &rect, const void *pixels)>;
	friend class view::SoftwareCanvas;

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

	TileCache(int canvasImplementation);
	~TileCache();

	TileCache(const TileCache &) = delete;
	TileCache(TileCache &&) = delete;
	TileCache &operator=(const TileCache &) = delete;
	TileCache &operator=(TileCache &&) = delete;

	QSize size() const;

	void clear();
	void resize(int width, int height, int offsetX, int offsetY);

	RenderResult render(int tileX, int tileY, const DP_Pixel8 *src);

	QImage toImage() const;
	QImage toSubImage(const QRect &rect);

	bool getResizeReset(Resize &outResize);
	bool needsDirtyCheck() const;
	void eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn);
	bool paintDirtyNavigatorTilesReset(bool all, QPixmap &cache);

private:
	class BaseImpl;
	class GlCanvasImpl;
	class SoftwareCanvasImpl;

	static BaseImpl *instantiateImpl(int canvasImplementation);
	const QVector<PixmapGrid::Cell> *pixmapCells() const;

	BaseImpl *d;
};

}

#endif
