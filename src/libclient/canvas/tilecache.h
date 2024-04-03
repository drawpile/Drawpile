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

namespace view {
class SoftwareCanvas;
}

namespace canvas {

// Holds rendered tiles in a format suitable for the canvas implementation and
// records changed tiles since the last render, both for the canvas itself and
// for the navigator. For the OpenGL canvas, the pixels are held in an array
// containing each tile in sequence, which is a format suitable for passing to
// glTexSubImage2D. Tiles at the right and bottom edge may have smaller
// dimensions, but they still allocate a full tile of space to avoid
// complicating the indexing. For the software canvas, it's a QPixmap.
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
	void paintDirtyNavigatorTilesReset(bool all, QPixmap &cache);

private:
	class BaseImpl;
	class GlCanvasImpl;
	class SoftwareCanvasImpl;

	static BaseImpl *instantiateImpl(int canvasImplementation);
	const QPixmap *softwareCanvasPixmap() const;

	BaseImpl *d;
};

}

#endif
