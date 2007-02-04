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
#include <QEvent>
#include <cmath>

#include "point.h"
#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), bg_(32,32), sizepressure_(false),
	opacitypressure_(false), hardnesspressure_(false), colorpressure_(false),
	shape_(Stroke)
{
	setMinimumSize(32,32);
	updateBackground();
}

void BrushPreview::setPreviewShape(PreviewShape shape)
{
	shape_ = shape;
	updatePreview();
	update();
}

void BrushPreview::setColor1(const QColor& color)
{
	color1_ = color;
	brush_.setColor(color);
	if(colorpressure_==false)
		brush_.setColor2(color);
	updatePreview();
	update();
}

void BrushPreview::setColor2(const QColor& color)
{
	color2_ = color;
	if(colorpressure_) {
		brush_.setColor2(color);
		updatePreview();
		update();
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	updatePreview();
}

void BrushPreview::changeEvent(QEvent *event)
{
	updateBackground();
	updatePreview();
	update();
}

void BrushPreview::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.drawImage(contentsRect().topLeft(), preview_);
}

void BrushPreview::updateBackground()
{
	QPainter painter(&bg_);
	bg_.fill(palette().light().color());
	QRectF rect(0,0,bg_.width()/2, bg_.height()/2);
	painter.fillRect(rect,palette().mid());
	rect.moveTo(rect.width(),rect.height());
	painter.fillRect(rect,palette().mid());
}

void BrushPreview::updatePreview()
{
	if(preview_.size() != contentsRect().size())
		preview_ = QImage(contentsRect().size(), QImage::Format_RGB32);

	// Paint background
	{
		QPainter painter(&preview_);
		painter.fillRect(QRect(0,0,preview_.width(),preview_.height()),
				bg_);
	}

	const int strokew = width() - width()/4;
	const int strokeh = height() / 4;
	const int offx = width()/8;
	const int offy = height()/2;
	const double dphase = (2*M_PI)/double(strokew);
	double phase = 0;
	for(int x=0;x<strokew;x++, phase += dphase) {
		const qreal fx = x/qreal(strokew);
		qreal pressure = ((fx*fx) - (fx*fx*fx))*6.756;
		if(pressure<0)
			pressure = 0;
		else if(pressure>1)
			pressure = 1;
		const int y = shape_==Stroke?(qRound(sin(phase) * strokeh)):0;
		brush_.draw(preview_,drawingboard::Point(offx+x,offy+y,pressure));
	}
}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const drawingboard::Brush& brush)
{
	brush_ = brush;
	updatePreview();
	update();
}

/**
 * @param size brush size
 */
void BrushPreview::setSize(int size)
{
	brush_.setRadius(size);
	if(sizepressure_==false)
		brush_.setRadius2(size);
	updatePreview();
	update();
}

/**
 * @param opacity brush opacity. Range is [0..100]
 */
void BrushPreview::setOpacity(int opacity)
{
	qreal o = opacity/100.0;
	brush_.setOpacity(o);
	if(opacitypressure_==false)
		brush_.setOpacity2(o);
	updatePreview();
	update();
}

/**
 * @param hardness brush hardness . Range is [0..100]
 */
void BrushPreview::setHardness(int hardness)
{
	qreal h = hardness/100.0;
	brush_.setHardness(h);
	if(hardnesspressure_==false)
		brush_.setHardness2(h);
	updatePreview();
	update();
}

void BrushPreview::setSizePressure(bool enable)
{
	sizepressure_ = enable;
	if(enable)
		brush_.setRadius2(0);
	else
		brush_.setRadius2(brush_.radius(1.0));
	updatePreview();
	update();
}

void BrushPreview::setOpacityPressure(bool enable)
{
	opacitypressure_ = enable;
	if(enable)
		brush_.setOpacity2(0);
	else
		brush_.setOpacity2(brush_.opacity(1.0));
	updatePreview();
	update();
}

void BrushPreview::setHardnessPressure(bool enable)
{
	hardnesspressure_ = enable;
	if(enable)
		brush_.setHardness2(0);
	else
		brush_.setHardness2(brush_.hardness(1.0));
	updatePreview();
	update();
}

void BrushPreview::setColorPressure(bool enable)
{
	colorpressure_ = enable;
	if(enable)
		brush_.setColor2(color2_);
	else
		brush_.setColor2(color1_);
	updatePreview();
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

