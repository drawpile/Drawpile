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
#include <QPointer>

namespace canvas {
	class PaintEngine;
}

namespace drawingboard {

/**
 * @brief A graphics item that draws a LayerStack
 */
class CanvasItem final : public QGraphicsObject
{
Q_OBJECT
public:
	CanvasItem(QGraphicsItem *parent=nullptr);
	void setPaintEngine(canvas::PaintEngine *pe);

	QRectF boundingRect() const override;

	void setViewportBounds(const QRectF viewportBounds);

private slots:
	void refreshImage(const QRect &area);
	void canvasResize();

protected:
	void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
	void updateVisibleArea();

	QPointer<canvas::PaintEngine> m_image;
	QRectF m_boundingRect;
	QRectF m_viewportBounds;
	QRect m_visibleArea;
};

}

#endif
