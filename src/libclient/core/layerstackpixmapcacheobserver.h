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
#ifndef LAYERSTACKPIXMAPCACHEOBSERVER_H
#define LAYERSTACKPIXMAPCACHEOBSERVER_H

#include "layerstackobserver.h"

#include <QObject>
#include <QPixmap>

namespace paintcore {

class LayerStackPixmapCacheObserver : public QObject, public LayerStackObserver
{
	Q_OBJECT
public:
	explicit LayerStackPixmapCacheObserver(QObject *parent=nullptr);

	/**
	 * @brief Get a reference to the underlying cache pixmap while makign sure at least the given area has been refreshed
	 * @param refreshArea
	 * @return
	 */
	const QPixmap &getPixmap(const QRect &refreshArea);

	//! Get a reference to the underlying cache pixmap while making sure the whole pixmap is refreshed
	const QPixmap &getPixmap();

signals:
	void areaChanged(const QRect &area) override;
	void resized(int xoffset, int yoffset, const QSize &oldSize) override;

private:
	QPixmap m_cache;
};

}

#endif
