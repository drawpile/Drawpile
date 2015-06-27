/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2015 Calle Laakkonen

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
#ifndef SELECTIONITEM_H
#define SELECTIONITEM_H

#include "core/blendmodes.h"

#include <QGraphicsItem>

namespace net {
	class Client;
}

namespace drawingboard {

class SelectionItem : public QGraphicsItem
{
public:
	static const int HANDLE = 10;
	enum { Type= UserType + 11 };
	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

	SelectionItem(QGraphicsItem *parent=0);

	void setRect(const QRect &rect);
	void setPolygon(const QPolygon &polygon);
	void translate(const QPoint &offset);
	void scale(qreal x, qreal y);

	QRect polygonRect() const { return _polygon.boundingRect().toRect(); }

	void resetPolygonShape();

	//! Get a mask image of the polygon
	QPair<QPoint,QImage> polygonMask(const QColor &color) const;

	//! Check if this selection is a non-rotated rectangle
	bool isAxisAlignedRectangle() const;

	void addPointToPolygon(const QPoint &point);
	void closePolygon();

	//! Get the translation handle at the point
	Handle handleAt(const QPoint &point, float zoom) const;

	//! Adjust selection position or size
	void adjustGeometry(Handle handle, const QPoint &delta, bool keepAspect);

	//! Rotate selection around its center by the given amount
	void rotate(float angle);

	//! Set the paste buffer
	void setPasteImage(const QImage &image);

	void setMovedFromCanvas(bool moved) { _movedFromCanvas = moved; }

	//! Get the paste buffer
	const QImage &pasteImage() const { return _pasteimg; }

	//! Did the pasted image come directly from the canvas?
	bool isMovedFromCanvas() const { return _movedFromCanvas; }

	//! reimplementation
	QRectF boundingRect() const;

	//! reimplementation
	int type() const { return Type; }

	void marchingAnts();

	//! Put the paste buffer to the canvas
	void pasteToCanvas(net::Client *client, int layer) const;

	//! Fill the selected pixels
	void fillCanvas(const QColor &color, paintcore::BlendMode::Mode mode, net::Client *client, int layer) const;

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

private:
	QRect canvasRect() const;

	void savePolygonShape();
	QPolygon polygon() const { return _polygon.toPolygon(); }
	void adjust(int dx1, int dy1, int dx2, int dy2);

	QPolygonF _polygon;
	QPolygonF _originalPolygonShape;
	qreal _marchingants;
	QImage _pasteimg;
	bool _closePolygon;
	bool _movedFromCanvas;
};

}

#endif // SELECTIONITEM_H
