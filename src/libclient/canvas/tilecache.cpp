// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/tile.h>
#include <dpengine/tile_iterator.h>
}
#include "libclient/canvas/tilecache.h"
#include "libclient/settings.h"
#include <QPainter>
#include <QPixmap>
#include <QtGlobal>
#include <QtMath>

namespace canvas {

class TileCache::BaseImpl {
public:
	virtual ~BaseImpl() {}

	QSize size() const { return QSize(m_width, m_height); }

	void clear()
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
		clearImpl();
	}

	void resize(int width, int height, int offsetX, int offsetY)
	{
		DP_TileCounts tc = DP_tile_counts_round(width, height);
		int tileTotal = tc.x * tc.y;
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
		resizeImpl(width, height, tileTotal);
	}

	bool getResizeReset(Resize &outResize)
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

	bool needsDirtyCheck() const { return m_needsDirtyCheck; }

	bool paintDirtyNavigatorTilesReset(bool all, QPixmap &cache)
	{
		QSize targetSize = cache.size();
		qreal ratioX = qreal(targetSize.width()) / qreal(m_width);
		qreal ratioY = qreal(targetSize.height()) / qreal(m_height);
		QPainter painter(&cache);
		painter.setCompositionMode(QPainter::CompositionMode_Source);
		bool changed = false;
		for(int tileY = 0; tileY < m_ytiles; ++tileY) {
			for(int tileX = 0; tileX < m_xtiles; ++tileX) {
				int i = tileIndex(tileX, tileY);
				bool &dirtyNavigatorTile = m_dirtyNavigatorTiles[i];
				if(all || dirtyNavigatorTile) {
					dirtyNavigatorTile = false;
					changed = true;
					QRect sourceRect = rectAt(tileX, tileY);
					QRect targetRect(
						qFloor(sourceRect.x() * ratioX),
						qFloor(sourceRect.y() * ratioY),
						qCeil(sourceRect.width() * ratioX),
						qCeil(sourceRect.height() * ratioY));
					paintNavigatorTileImpl(painter, i, sourceRect, targetRect);
				}
			}
		}
		m_needsNavigatorDirtyCheck = false;
		return changed;
	}

	virtual const QPixmap *pixmap() { return nullptr; }

	virtual RenderResult render(int tileX, int tileY, const DP_Pixel8 *src) = 0;
	virtual QImage toImage() = 0;
	virtual QImage toSubImage(const QRect &rect) = 0;
	virtual void
	eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn) = 0;

protected:
	int tileIndex(int tileX, int tileY) const
	{
		return tileY * m_xtiles + tileX;
	}

	QRect rectAt(int tileX, int tileY) const
	{
		return QRect(
			tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE,
			tileX < m_lastTileX ? DP_TILE_SIZE : m_lastWidth,
			tileY < m_lastTileY ? DP_TILE_SIZE : m_lastHeight);
	}

	virtual void clearImpl() = 0;
	virtual void resizeImpl(int width, int height, int tileTotal) = 0;
	virtual void paintNavigatorTileImpl(
		QPainter &painter, int i, const QRect &sourceRect,
		const QRect &targetRect) = 0;

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

class TileCache::GlCanvasImpl : public BaseImpl {
public:
	~GlCanvasImpl() override { DP_free(m_pixels); }

	RenderResult render(int tileX, int tileY, const DP_Pixel8 *src) override
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

	QImage toImage() override
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
					QRect(
						tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE, tileW,
						tileH),
					QImage(
						reinterpret_cast<const uchar *>(
							pixelsAt(tileIndex(tileX, tileY))),
						tileW, tileH, QImage::Format_ARGB32_Premultiplied));
			}
		}
		painter.end();
		return img;
	}

	QImage toSubImage(const QRect &rect) override
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
			DP_TileIterator ti = DP_tile_iterator_make(
				m_width, m_height, DP_rect_make(x, y, w, h));
			while(DP_tile_iterator_next(&ti)) {
				DP_TileIntoDstIterator tid =
					DP_tile_into_dst_iterator_make(&ti);
				const DP_Pixel8 *tilePixels =
					pixelsAt(tileIndex(ti.col, ti.row));
				int tileWidth =
					ti.col < m_lastTileX ? DP_TILE_SIZE : m_lastWidth;
				while(DP_tile_into_dst_iterator_next(&tid)) {
					imgPixels[tid.dst_y * w + tid.dst_x] =
						tilePixels[tid.tile_y * tileWidth + tid.tile_x].color;
				}
			}
			return img;
		}
	}

	void eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn) override
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

protected:
	void clearImpl() override
	{
		DP_free(m_pixels);
		m_pixels = nullptr;
		m_capacity = 0;
	}

	void resizeImpl(int width, int height, int tileTotal) override
	{
		Q_UNUSED(width);
		Q_UNUSED(height);
		size_t requiredCapacity =
			size_t(tileTotal) * DP_TILE_LENGTH * sizeof(*m_pixels);
		if(m_capacity == requiredCapacity) {
			memset(m_pixels, 0, requiredCapacity);
		} else {
			DP_free(m_pixels);
			m_pixels =
				static_cast<DP_Pixel8 *>(DP_malloc_zeroed(requiredCapacity));
			m_capacity = requiredCapacity;
		}
	}

	void paintNavigatorTileImpl(
		QPainter &painter, int i, const QRect &sourceRect,
		const QRect &targetRect) override
	{
		painter.drawImage(
			targetRect, QImage(
							reinterpret_cast<const uchar *>(pixelsAt(i)),
							sourceRect.width(), sourceRect.height(),
							QImage::Format_ARGB32_Premultiplied));
	}

private:
	DP_Pixel8 *pixelsAt(int i) const { return m_pixels + i * DP_TILE_LENGTH; }

	DP_Pixel8 *m_pixels = nullptr;
	size_t m_capacity = 0;
};

class TileCache::SoftwareCanvasImpl : public BaseImpl {
public:
	~SoftwareCanvasImpl() override {}

	RenderResult render(int tileX, int tileY, const DP_Pixel8 *src) override
	{
		int w = tileX < m_lastTileX ? DP_TILE_SIZE : m_lastWidth;
		int h = tileY < m_lastTileY ? DP_TILE_SIZE : m_lastHeight;
		{
			QPainter painter(&m_pixmap);
			painter.setCompositionMode(QPainter::CompositionMode_Source);
			painter.drawImage(
				QRect(tileX * DP_TILE_SIZE, tileY * DP_TILE_SIZE, w, h),
				QImage(
					reinterpret_cast<const uchar *>(src), DP_TILE_SIZE,
					DP_TILE_SIZE, QImage::Format_ARGB32_Premultiplied),
				QRect(0, 0, w, h));
		}

		RenderResult result;
		int i = tileIndex(tileX, tileY);
		bool &dirtyTile = m_dirtyTiles[i];
		if(!dirtyTile) {
			dirtyTile = true;
			m_needsDirtyCheck = true;
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

	QImage toImage() override
	{
		return m_pixmap.toImage().convertToFormat(
			QImage::Format_ARGB32_Premultiplied);
	}

	QImage toSubImage(const QRect &rect) override
	{
		QRect r = rect.intersected(QRect(0, 0, m_width, m_height));
		return r.isEmpty() ? QImage()
						   : m_pixmap.copy(r).toImage().convertToFormat(
								 QImage::Format_ARGB32_Premultiplied);
	}

	void eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn) override
	{
		int right = qMin(tileArea.right(), m_xtiles - 1);
		int bottom = qMin(tileArea.bottom(), m_ytiles - 1);
		for(int tileY = tileArea.top(); tileY <= bottom; ++tileY) {
			for(int tileX = tileArea.left(); tileX <= right; ++tileX) {
				int i = tileIndex(tileX, tileY);
				bool &dirtyTile = m_dirtyTiles[i];
				if(dirtyTile) {
					dirtyTile = false;
					fn(rectAt(tileX, tileY), nullptr);
				}
			}
		}
		m_needsDirtyCheck = false;
	}

	const QPixmap *pixmap() override { return &m_pixmap; }

protected:
	void clearImpl() override { m_pixmap = QPixmap(); }

	void resizeImpl(int width, int height, int tileTotal) override
	{
		Q_UNUSED(tileTotal);
		if(width != m_pixmap.width() || height != m_pixmap.height()) {
			m_pixmap = QPixmap(width, height);
		}
		m_pixmap.fill(Qt::transparent);
	}

	void paintNavigatorTileImpl(
		QPainter &painter, int i, const QRect &sourceRect,
		const QRect &targetRect) override
	{
		Q_UNUSED(i);
		painter.drawPixmap(targetRect, m_pixmap, sourceRect);
	}

private:
	QPixmap m_pixmap;
};

TileCache::TileCache(int canvasImplementation)
	: d(instantiateImpl(canvasImplementation))
{
}

TileCache::~TileCache()
{
	delete d;
}

QSize TileCache::size() const
{
	return d->size();
}

void TileCache::clear()
{
	d->clear();
}

void TileCache::resize(int width, int height, int offsetX, int offsetY)
{
	Q_ASSERT(width >= 0);
	Q_ASSERT(height >= 0);
	d->resize(width, height, offsetX, offsetY);
}

TileCache::RenderResult
TileCache::render(int tileX, int tileY, const DP_Pixel8 *src)
{
	return d->render(tileX, tileY, src);
}

QImage TileCache::toImage() const
{
	return d->toImage();
}

QImage TileCache::toSubImage(const QRect &rect)
{
	return d->toSubImage(rect);
}

bool TileCache::getResizeReset(Resize &outResize)
{
	return d->getResizeReset(outResize);
}

bool TileCache::needsDirtyCheck() const
{
	return d->needsDirtyCheck();
}

void TileCache::eachDirtyTileReset(const QRect &tileArea, const OnTileFn &fn)
{
	d->eachDirtyTileReset(tileArea, fn);
}

bool TileCache::paintDirtyNavigatorTilesReset(bool all, QPixmap &cache)
{
	return d->paintDirtyNavigatorTilesReset(all, cache);
}

TileCache::BaseImpl *TileCache::instantiateImpl(int canvasImplementation)
{
	using libclient::settings::CanvasImplementation;
	switch(canvasImplementation) {
	case int(CanvasImplementation::OpenGl):
		return new GlCanvasImpl;
	case int(CanvasImplementation::Software):
		return new SoftwareCanvasImpl;
	default:
		return nullptr;
	}
}

const QPixmap *TileCache::softwareCanvasPixmap() const
{
	return d->pixmap();
}

}
