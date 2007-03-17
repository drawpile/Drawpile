/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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

#include <QPainter>
#include <QStyle>
#include <QStyleOptionFocusRect>
#include <QStyleOptionFrameV2>
#include <QMouseEvent>

#include "gradientslider.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

GradientSlider::GradientSlider(QWidget *parent)
	: QAbstractSlider(parent), color1_(Qt::black), color2_(Qt::white),
	saturation_(1), value_(1), mode_(Color)
{
}

void GradientSlider::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	// Draw the frame
#if 0
	QStyleOptionFrameV2 frame;
	frame.initFrom(this);
	frame.midLineWidth = border_;
	frame.state = QStyle::State_Sunken;

	style()->drawPrimitive(QStyle::PE_Frame, &frame, &painter, this);
#endif

	// Draw the gradient
	QPointF endpoint;
	QRect gradrect;
	if(orientation() == Qt::Horizontal) {
		endpoint = QPointF(width(),0);
		gradrect = contentsRect().adjusted(0,height()/4+1,-1,-height()/4 - 2);
	} else {
		endpoint = QPointF(0,height());
		gradrect = contentsRect().adjusted(width()/4-1,0,-width()/4 - 2,-1);
	}

	QLinearGradient grad(QPointF(0,0),endpoint);

	if(mode_ == Color) {
		grad.setColorAt(0, color1_);
		grad.setColorAt(1, color2_);
	} else {
		for(int i=0;i<=7;++i)
			grad.setColorAt(i/7.0,
					QColor::fromHsvF(i/7.001, saturation_, value_));
	}

	painter.fillRect(gradrect, QBrush(grad));
	painter.setPen(palette().color(QPalette::Mid));
	painter.drawLine(gradrect.topLeft(), gradrect.bottomLeft());
	painter.drawLine(gradrect.topLeft(), gradrect.topRight());
	painter.setPen(palette().color(QPalette::Light));
	painter.drawLine(gradrect.topRight(), gradrect.bottomRight());
	painter.drawLine(gradrect.bottomLeft(), gradrect.bottomRight());

	// Draw pointer
	int pos = qRound((value() - minimum()) / qreal(maximum()-minimum()) *
		(((orientation()==Qt::Horizontal)?width():height())));

	QPoint points[3];

	if(orientation() == Qt::Horizontal) {
		int w = height()/4;
		points[0] = QPoint(pos-w,0);
		points[1] = QPoint(pos+w,0);
		points[2] = QPoint(pos,w);
	} else {
		int h = width()/4;
		points[0] = QPoint(0, pos-h);
		points[1] = QPoint(0, pos+h);
		points[2] = QPoint(h, pos);
	}
	painter.setPen(palette().color(QPalette::Dark));
	painter.drawPolygon(points,3);

	// Focus rectangle
	if(hasFocus()) {
		painter.setClipRect(contentsRect());
		QStyleOptionFocusRect option;
		option.initFrom(this);
		option.backgroundColor = palette().color(QPalette::Background);

		style()->drawPrimitive(QStyle::PE_FrameFocusRect, &option, &painter, this);
	}

}

void GradientSlider::mousePressEvent(QMouseEvent *event)
{
	setPosition(event->x(),event->y());
	update();
}

void GradientSlider::mouseMoveEvent(QMouseEvent *event)
{
	setPosition(event->x(),event->y());
	update();
}

void GradientSlider::setPosition(int x,int y)
{
	qreal pos;
	if(orientation() == Qt::Horizontal)
		pos = x / qreal(width());
	else
		pos = y / qreal(height());
	setValue(qRound(minimum() + pos * (maximum()-minimum())));
}

void GradientSlider::setMode(Mode mode)
{
	mode_ = mode;
	update();
}

void GradientSlider::setColor1(const QColor& color)
{
	color1_ = color;
	update();
}

void GradientSlider::setColor2(const QColor& color)
{
	color2_ = color;
	update();
}

void GradientSlider::setColorSaturation(qreal saturation)
{
	if(saturation<0)
		saturation = 0;
	else if(saturation>1)
		saturation = 1;
	saturation_ = saturation;
	update();
}

void GradientSlider::setColorValue(qreal value)
{
	if(value<0)
		value = 0;
	else if(value>1)
		value = 1;
	value_ = value;
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

