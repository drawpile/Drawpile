/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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

#include <QGraphicsItem>

namespace drawingboard {


class SelectionItem : public QGraphicsItem
{
public:
	static const int HANDLE = 10;
	enum { Type= UserType + 11 };
	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

	SelectionItem(QGraphicsItem *parent=0);

	void setRect(const QRect &rect);

	QRect rect() const { return _rect; }

	//! Get the translation handle at the point
	Handle handleAt(const QPoint &point) const;

	//! Adjust selection position or size
	void adjustGeometry(Handle handle, const QPoint &delta);

	//! Set the paste buffer
	void setPasteImage(const QImage &image);

	//! Get the paste buffer
	const QImage &pasteImage() const { return _pasteimg; }

	//! reimplementation
	QRectF boundingRect() const;

	//! reimplementation
	int type() const { return Type; }

	void marchingAnts();

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

private:
	QRect _rect;
	qreal _marchingants;
	QImage _pasteimg;
};

}

#endif // SELECTIONITEM_H
