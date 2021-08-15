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

#ifndef LAYERSTACKPIXMAP_H
#define LAYERSTACKPIXMAP_H

#include <QObject>
#include <QPixmap>

namespace rustpile {
	struct PaintEngine;
	struct Rectangle;
	struct Size;
}

namespace canvas {

/**
 * @brief A pixmap cache of the canvas content
 */
class PaintEnginePixmap : public QObject {
	Q_OBJECT
public:
	explicit PaintEnginePixmap(QObject *parent=nullptr);

	void setPaintEngine(rustpile::PaintEngine *pe);

	/**
	 * @brief Get a reference to the underlying cache pixmap while makign sure at least the given area has been refreshed
	 */
	const QPixmap &getPixmap(const QRect &refreshArea);

	//! Get a reference to the underlying cache pixmap while making sure the whole pixmap is refreshed
	const QPixmap &getPixmap();

	//! Get the current size of the canvas
	QSize size() const;

signals:
	void areaChanged(const QRect &area);
	void resized(int xoffset, int yoffset, const QSize &oldSize);

private:
	rustpile::PaintEngine *m_pe;
	QPixmap m_cache;

	friend void paintEngineAreaChanged(void *pep, rustpile::Rectangle area);
	friend void paintEngineResized(void *pep, int xoffset, int yoffset, rustpile::Size oldSize);
};

}

#endif

