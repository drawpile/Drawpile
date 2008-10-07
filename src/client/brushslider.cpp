/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2008 Calle Laakkonen

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
#include <QMouseEvent>

#include "brushslider.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushSlider::BrushSlider(QWidget *parent)
	: QAbstractSlider(parent), style_(Size)
{
}

void BrushSlider::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.setClipRegion(event->region());
	painter.setRenderHint(QPainter::Antialiasing);
	QPainterPath path;

	qreal dia = contentsRect().height() - 1;
	qreal w = contentsRect().width() - 1;

	path.moveTo(dia+dia/2.0, 0);
	path.cubicTo(QPointF(dia, 0), QPointF(dia, dia),
			QPointF(dia+dia/2.0, dia));
	path.lineTo(w-dia-dia/2.0, dia);
	path.cubicTo(QPointF(w-dia, dia), QPointF(w-dia, 0),
			QPointF(w-dia-dia/2, 0));
	path.closeSubpath();

	// Draw the background
	painter.fillPath(path, palette().base());
	painter.setPen(QPen(palette().color(QPalette::Mid)));
	painter.drawPath(path);

	// Draw the brush circles
	drawCircle(painter, dia, 0.5, 0.5, 0.0);
	drawCircle(painter, dia, w-dia, 0.5, 1.0);
	qreal pos = (sliderPosition() - minimum()) / qreal(maximum()-minimum());
	qreal slider = dia+(w-dia*3)*pos;
	drawCircle(painter, dia, slider, 0.5, pos);

	// Draw the value text
	QFont fnt = font();
	fnt.setPixelSize(qRound(dia*0.7));
	painter.setFont(fnt);
	
	if(pos > 0.5) {
		painter.drawText(QRectF(dia+dia/2, 1, w, dia),
				Qt::AlignLeft|Qt::AlignVCenter,
				QString::number(value()) + suffix_);
	} else {
		painter.drawText(QRectF(0, 1, w-dia-dia/2, dia),
				Qt::AlignRight|Qt::AlignVCenter,
				QString::number(value()) + suffix_);
	}
}

void BrushSlider::drawCircle(QPainter& painter, qreal dia, qreal x,
		qreal y, qreal value)
{
	const QRectF rect(x, y, dia, dia);
	painter.setPen(QPen());
	switch(style_) {
		case Size: {
			painter.setBrush(palette().base());
			painter.setPen(QPen(palette().color(QPalette::Mid)));
			painter.drawEllipse(rect);
			const qreal adj = (dia - dia*value)/2.0;
			const QRectF r2 = rect.adjusted(adj,adj,-adj,-adj);
			painter.setBrush(palette().buttonText());
			painter.drawEllipse(r2);
				   } return;
		case Opacity: {
			const QColor col1(palette().color(QPalette::Base));
			const QColor col2(palette().color(QPalette::ButtonText));
			painter.setBrush(QColor(
				col1.red() + qRound((col2.red()-col1.red()) * value),
				col1.green() + qRound((col2.green()-col1.green()) * value),
				col1.blue() + qRound((col2.blue()-col1.blue()) * value)));
					  } break;
		case Hardness:
			if(value>=1) {
				painter.setBrush(palette().buttonText());
			} else {
				QRadialGradient gradient(x+dia/2.0, y+dia/2.0, dia/2.0);
				gradient.setColorAt(0, palette().color(QPalette::ButtonText));
				gradient.setColorAt(value,
						palette().color(QPalette::ButtonText));
				gradient.setColorAt(1, palette().color(QPalette::Base));
				painter.setBrush(QBrush(gradient));
			}
			break;
		case Spacing: {
			painter.setPen(QPen(palette().color(QPalette::Mid)));
			qreal adj = dia/4.0;
			qreal off = adj/1.0;
			painter.drawEllipse(rect.adjusted(
						adj-off*value, adj, -adj-off*value, -adj));

			painter.drawEllipse(rect.adjusted(
						adj+off*value, adj, -adj+off*value, -adj));
					  } return; // this is a special case

	}
	painter.setPen(QPen(palette().color(QPalette::Mid)));
	painter.drawEllipse(rect);
}

void BrushSlider::mousePressEvent(QMouseEvent *event)
{
	qreal dia = contentsRect().height() - 1;
	if(event->x() < dia) {
		if(value() > minimum())
			setValue(value()-1);
		click_ = BUTTON;
	} else if(event->x()>contentsRect().width()-dia) {
		if(value() < maximum())
			setValue(value()+1);
		click_ = BUTTON;
	} else {
		int handlex = qRound(dia + (value()-minimum()) / qreal(maximum()-minimum()) * (contentsRect().width() - dia*3));
		if(event->x() >= handlex && event->x()<=handlex+dia) {
			click_ = HANDLE;
		} else {
			int adjust = (maximum()-minimum())/10;
			if(event->x()>handlex)
				adjust = -adjust;
			setValue(qBound(minimum(), value() - adjust, maximum()));
			click_ = SLIDER;
		}
		//setPosition(event->x(),event->y());
		update();
	}
}

void BrushSlider::mouseMoveEvent(QMouseEvent *event)
{
	if(click_ == HANDLE) {
		setPosition(event->x(), event->y());
		update();
	}
}

void BrushSlider::setPosition(int x, int y)
{
	Q_UNUSED(y); // TODO vertical mode
	qreal dia = contentsRect().height() - 1;
	qreal val = qBound(0.0, (x - dia*1.5) / (contentsRect().width() - dia*3.0), 1.0);
	setValue(qRound(minimum() + val * (maximum()-minimum())));
}

void BrushSlider::setStyle(Style style)
{
	style_ = style;
	update();
}

void BrushSlider::setSuffix(const QString& suffix) {
	suffix_ = suffix;
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

