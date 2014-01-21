/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include <QApplication>
#include <QPainter>
#include <QFontMetrics>
#include <QPainterPath>
#include <QGraphicsDropShadowEffect>

#include "usermarkeritem.h"

namespace drawingboard {


namespace {
static const float ARROW = 10;

}
UserMarkerItem::UserMarkerItem(QGraphicsItem *parent)
	: QGraphicsObject(parent), _fadeout(0)
{
	setFlag(ItemIgnoresTransformations);
	_bgbrush.setStyle(Qt::SolidPattern);
	QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect;
	shadow->setOffset(0);
	shadow->setBlurRadius(10);
	setGraphicsEffect(shadow);
	setZValue(9999);
}

void UserMarkerItem::setColor(const QColor &color)
{
	_bgbrush.setColor(color);

	if ((color.red() * 30) + (color.green() * 59) + (color.blue() * 11) > 12800)
		_textpen = QPen(Qt::black);
	else
		_textpen = QPen(Qt::white);

	update();
}

void UserMarkerItem::setText(const QString &text)
{
	_text = text;

	// Make a new bubble for the text
	QRect textrect = qApp->fontMetrics().boundingRect(text);

	const float round = 3;
	const float padding = 5;
	const float width = qMax((ARROW+round)*2, textrect.width() + 2*padding);
	const float rad = width / 2.0;
	const float height = textrect.height() + ARROW + 2 * padding;

	_bounds = QRectF(-rad, -height, width, height);

	_textpos = QPointF(-rad + padding, -ARROW - padding);

	_bubble = QPainterPath(QPointF(0, 0));

	_bubble.lineTo(-ARROW, -ARROW);
	_bubble.lineTo(-rad+round, -ARROW);

	_bubble.quadTo(-rad, -ARROW, -rad, -ARROW-round);
	_bubble.lineTo(-rad, -height+round);
	_bubble.quadTo(-rad, -height, -rad+round, -height);

	_bubble.lineTo(rad-round, -height);
	_bubble.quadTo(rad, -height, rad, -height+round);
	_bubble.lineTo(rad, -ARROW-round);

	_bubble.quadTo(rad, -ARROW, rad-round, -ARROW);
	_bubble.lineTo(ARROW, -ARROW);

	_bubble.closeSubpath();
}

QRectF UserMarkerItem::boundingRect() const
{
	return _bounds;
}

void UserMarkerItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setPen(Qt::NoPen);

	painter->setBrush(_bgbrush);
	painter->drawPath(_bubble);

	painter->setFont(qApp->font());
	painter->setPen(_textpen);
	painter->drawText(_textpos, _text);
}

void UserMarkerItem::timerEvent(QTimerEvent *)
{
	qreal o = opacity() - 1/(25.0 *5);
	if(o<=0) {
		hide();
		killTimer(_fadeout);
		_fadeout=0;
	} else
		setOpacity(o);
}

void UserMarkerItem::fadein()
{
	if(_fadeout) {
		killTimer(_fadeout);
		_fadeout=0;
	}
	setOpacity(1);
	show();
}

void UserMarkerItem::fadeout()
{
	if(isVisible() && _fadeout==0)
		_fadeout = startTimer(1000/25);
}

}
