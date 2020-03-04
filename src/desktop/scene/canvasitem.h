/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

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
#ifndef DP_CANVASITEM_H
#define DP_CANVASITEM_H

#include <QGraphicsObject>

namespace canvas {
	class PaintEnginePixmap;
}

namespace drawingboard {

/**
 * @brief A graphics item that draws a LayerStack
 */
class CanvasItem : public QGraphicsObject
{
Q_OBJECT
public:
	//! Construct an empty board
	CanvasItem(canvas::PaintEnginePixmap *image, QGraphicsItem *parent=nullptr);

	QRectF boundingRect() const override;

private slots:
	void refreshImage(const QRect &area);
	void canvasResize();

protected:
	void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
	canvas::PaintEnginePixmap *m_image;
};

}

#endif
