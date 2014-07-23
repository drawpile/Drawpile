/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2013-2014 Calle Laakkonen

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
#ifndef USERMARKERITEM_H
#define USERMARKERITEM_H

#include <QGraphicsObject>
#include <QPainterPath>

namespace drawingboard {

class UserMarkerItem : public QGraphicsItem
{
public:
	UserMarkerItem(QGraphicsItem *parent=0);

	QRectF boundingRect() const;

	void setColor(const QColor &color);
	const QColor &color() const;

	void setText(const QString &text);
	void setSubtext(const QString &text);

	void fadein();
	void fadeout();

	bool fadeoutStep(float dt);

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *);

private:
	void updateFullText();
	QRectF _bounds;
	QBrush _bgbrush;
	QPen _textpen;
	QPainterPath _bubble;
	QRectF _textrect;
	float _fadeout;

	QString _text1, _text2;
	QString _fulltext;
};

}

#endif // USERMARKERITEM_H
