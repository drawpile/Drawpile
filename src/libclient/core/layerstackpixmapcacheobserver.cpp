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

#include "layerstackpixmapcacheobserver.h"
#include "layerstack.h"

namespace paintcore {

LayerStackPixmapCacheObserver::LayerStackPixmapCacheObserver(QObject *parent)
	: QObject(parent), LayerStackObserver()
{
}

const QPixmap &LayerStackPixmapCacheObserver::getPixmap()
{
	if(!layerStack())
		return m_cache;

	return getPixmap(QRect(QPoint(), layerStack()->size()));
}

const QPixmap &LayerStackPixmapCacheObserver::getPixmap(const QRect &refreshArea)
{
	if(!layerStack())
		return m_cache;

	const QSize size = layerStack()->size();

	if((m_cache.isNull() || m_cache.size() != size) && size.isValid()) {
		m_cache = QPixmap(size);
		m_cache.fill();
	}

	paintChangedTiles(refreshArea & m_cache.rect(), &m_cache);

	return m_cache;
}

}
