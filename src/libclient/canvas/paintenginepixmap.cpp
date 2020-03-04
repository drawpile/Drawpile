/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2021 Calle Laakkonen

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

#include "paintenginepixmap.h"
#include "../../rustpile/rustpile.h"

#include <QPainter>

namespace {
static const int TILE_SIZE = 64; // FIXME
static void updateCacheTile(void *p, int x, int y, const uchar *pixels)
{
	qInfo("Updating tile at %d, %d", x, y);
	((QPainter*)p)->drawImage(
			x,
			y,
			QImage(pixels,
				TILE_SIZE, TILE_SIZE,
				QImage::Format_ARGB32_Premultiplied
			)
	);
}

}

namespace canvas {

PaintEnginePixmap::PaintEnginePixmap(rustpile::PaintEngine *pe, QObject *parent)
	: QObject(parent), m_pe(pe)
{
}

void PaintEnginePixmap::setPaintEngine(rustpile::PaintEngine *pe)
{
	m_pe = pe;
	m_cache = QPixmap();
}

const QPixmap& PaintEnginePixmap::getPixmap(const QRect &refreshArea)
{
	if(!m_pe)
		return m_cache;

	const auto size = this->size();
	if(!size.isValid())
		return m_cache;

	if(m_cache.isNull() || m_cache.size() != size) {
		m_cache = QPixmap(size);
		m_cache.fill();
	}

	const rustpile::Rectangle r {
		refreshArea.x(),
		refreshArea.y(),
		refreshArea.width(),
		refreshArea.height()
	};

	qInfo("getPixmap(%d, %d, %d, %d)", r.x, r.y, r.w, r.h);
	QPainter painter(&m_cache);
	painter.setCompositionMode(QPainter::CompositionMode_Source);
	rustpile::paintengine_paint_changes(m_pe, &painter, r, &updateCacheTile);

	return m_cache;
}

const QPixmap& PaintEnginePixmap::getPixmap()
{
	return getPixmap(QRect{-1, -1, -1, -1});
}

QSize PaintEnginePixmap::size() const
{
	if(!m_pe)
		return QSize();

	const auto size = rustpile::paintengine_canvas_size(m_pe);
	return QSize(size.width, size.height);
}


}
