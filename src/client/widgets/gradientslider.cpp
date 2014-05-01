/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include <QtGlobal>
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
		gradrect = contentsRect().adjusted(0,height()/8+1,-1,-height()/8 - 2);
	} else {
		endpoint = QPointF(0,height());
		gradrect = contentsRect().adjusted(width()/8-1,0,-width()/8 - 2,-1);
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
	const int pos = qRound((value() - minimum()) / qreal(maximum()-minimum()) *
		(((orientation()==Qt::Horizontal)?width():height())));

	QPoint points[3+3];

	if(orientation() == Qt::Horizontal) {
		const int w = height()/4;
		points[0] = QPoint(pos-w,gradrect.y()+1);
		points[1] = QPoint(pos+w,gradrect.y()+1);
		points[2] = QPoint(pos,gradrect.y()+w+1);

		points[3] = QPoint(pos-w,gradrect.bottom());
		points[4] = QPoint(pos+w,gradrect.bottom());
		points[5] = QPoint(pos,gradrect.bottom()-w);
	} else {
		const int h = width()/4;
		points[0] = QPoint(gradrect.x()+1, pos-h);
		points[1] = QPoint(gradrect.x()+1, pos+h);
		points[2] = QPoint(gradrect.x()+h+1, pos);

		points[3] = QPoint(gradrect.right(), pos-h);
		points[4] = QPoint(gradrect.right(), pos+h);
		points[5] = QPoint(gradrect.right()-h, pos);
	}
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::black);
	painter.drawConvexPolygon(points,3);
	painter.setBrush(Qt::white);
	painter.drawConvexPolygon(points+3,3);

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
	saturation_ = qBound(0.0, saturation, 1.0);
	update();
}

void GradientSlider::setColorValue(qreal value)
{
	value_ = qBound(0.0, value, 1.0);
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

