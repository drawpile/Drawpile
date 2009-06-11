/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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
#include <QPaintEvent>
#include <QPainter>
#include <QEvent>
#include <cmath>

#include "core/point.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), preview_(0), bg_(32,32), sizepressure_(false),
	opacitypressure_(false), hardnesspressure_(false), colorpressure_(false),
	shape_(Stroke)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);
}

BrushPreview::~BrushPreview() {
	delete preview_;
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
	if(colorpressure_)
		brush_.setColor2(color);
	updatePreview();
	update();
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	updatePreview();
}

void BrushPreview::changeEvent(QEvent *event)
{
	updatePreview();
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	preview_->paint(event->rect(), &painter);
}

void BrushPreview::updatePreview()
{
	if(preview_==0) {
		preview_ = new dpcore::LayerStack();
		preview_->addLayer("", QColor(0,0,0), contentsRect().size());
	} else if(preview_->width() != contentsRect().width() || preview_->height() != contentsRect().height()) {
		// TODO resize more nicely
		delete preview_;
		preview_ = new dpcore::LayerStack();
		preview_->addLayer("", QColor(0,0,0), contentsRect().size());
	}
	dpcore::Layer *layer = preview_->getLayerByIndex(0);

	//layer->fillChecker(palette().light().color(), palette().mid().color());
	layer->fillColor(color2_);

	const int strokew = preview_->width() - preview_->width()/4;
	const int strokeh = preview_->height() / 4;
	const int offx = preview_->width()/8;
	const int offy = preview_->height()/2;
	qreal distance = 0;
	if(shape_ == Stroke) {
		int lastx=0,lasty=0;
		qreal lastp = 0;
		const qreal dphase = (2*M_PI)/qreal(strokew);
		qreal phase = 0;
		for(int x=0;x<strokew;++x, phase += dphase) {

			const qreal fx = x/qreal(strokew);
			const qreal pressure = qBound(0.0, ((fx*fx) - (fx*fx*fx))*6.756, 1.0);
			const int y = qRound(sin(phase) * strokeh);
			layer->drawLine(brush_,
					dpcore::Point(offx+lastx,offy+lasty, lastp),
					dpcore::Point(offx+x, offy+y, pressure), distance);
			lastx = x;
			lasty = y;
			lastp = pressure;
		}
	} else if(shape_ == Line) {
		layer->drawLine(brush_,
				dpcore::Point(offx, offy, 1),
				dpcore::Point(offx+strokew, offy, 1),
				distance
				);
	} else {
		layer->drawLine(brush_,
				dpcore::Point(offx, offy-strokeh, 1),
				dpcore::Point(offx+strokew, offy-strokeh, 1),
				distance);
		layer->drawLine(brush_,
				dpcore::Point(offx+strokew, offy-strokeh, 1),
				dpcore::Point(offx+strokew, offy+strokeh, 1),
				distance);
		layer->drawLine(brush_,
				dpcore::Point(offx+strokew, offy+strokeh, 1),
				dpcore::Point(offx, offy+strokeh, 1),
				distance);
		layer->drawLine(brush_,
				dpcore::Point(offx, offy+strokeh, 1),
				dpcore::Point(offx, offy-strokeh, 1),
				distance);
	}
}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const dpcore::Brush& brush)
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
 * @param opacity brush opacity
 * @pre 0 <= opacity <= 100
 */
void BrushPreview::setOpacity(int opacity)
{
	const qreal o = opacity/100.0;
	brush_.setOpacity(o);
	if(opacitypressure_==false)
		brush_.setOpacity2(o);
	updatePreview();
	update();
}

/**
 * @param hardness brush hardness
 * @pre 0 <= hardness <= 100
 */
void BrushPreview::setHardness(int hardness)
{
	const qreal h = hardness/100.0;
	brush_.setHardness(h);
	if(hardnesspressure_==false)
		brush_.setHardness2(h);
	updatePreview();
	update();
}

/**
 * @param spacing dab spacing
 * @pre 0 <= spacing <= 100
 */
void BrushPreview::setSpacing(int spacing)
{
	brush_.setSpacing(spacing);
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

void BrushPreview::setSubPixel(bool enable)
{
	brush_.setSubPixel(enable);
	updatePreview();
	update();
}

void BrushPreview::setBlendingMode(int mode)
{
	// Eraser mode (0) is not visible, so add 1
	brush_.setBlendingMode(mode + 1);
	updatePreview();
	update();
}

void BrushPreview::setHardEdge(bool hard)
{
	if(hard) {
		oldhardness1_ = brush_.hardness(0);
		oldhardness2_ = brush_.hardness(1);
		brush_.setHardness(1);
		brush_.setHardness2(1);
		brush_.setSubPixel(false);
	} else {
		brush_.setHardness(oldhardness1_);
		brush_.setHardness2(oldhardness2_);
		brush_.setSubPixel(true);
	}
	updatePreview();
	update();
}

#ifndef DESIGNER_PLUGIN
}
#endif

