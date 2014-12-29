/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

namespace paintcore {
	class LayerStack;
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
	CanvasItem(QGraphicsItem *parent=0);

	//! Get the image
	paintcore::LayerStack *image() const { return _image; }

	/** reimplematation */
	QRectF boundingRect() const;

public slots:
	void refreshImage(const QRect &area);

private slots:
	void canvasResize();

protected:
	/** reimplementation */
	void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*);

private:
	paintcore::LayerStack *_image;
	QPixmap _cache;
};

}

#endif
