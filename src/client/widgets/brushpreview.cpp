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
#include <QDebug>

#include <QPaintEvent>
#include <QPainter>
#include <QEvent>
#include <QMenu>

#include "core/point.h"
#include "core/layerstack.h"
#include "core/layer.h"
#include "core/shapes.h"
#include "core/floodfill.h"
#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), preview_(0), sizepressure_(false),
	opacitypressure_(false), hardnesspressure_(false), colorpressure_(false),
	color1_(Qt::black), color2_(Qt::white),
	shape_(Stroke), _fillTolerance(0), _fillExpansion(0), _tranparentbg(false)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);

	_ctxmenu = new QMenu(this);

	_ctxmenu->addAction(tr("Change Foreground Color"), this, SIGNAL(requestFgColorChange()));
	_ctxmenu->addAction(tr("Change Background Color"), this, SIGNAL(requestBgColorChange()));
}

BrushPreview::~BrushPreview() {
	delete preview_;
}

void BrushPreview::setPreviewShape(PreviewShape shape)
{
	shape_ = shape;
	_needupdate = true;
	update();
}

void BrushPreview::setTransparentBackground(bool transparent)
{
	_tranparentbg = transparent;
	_needupdate = true;
	update();
}

void BrushPreview::setColor1(const QColor& color)
{
	color1_ = color;
	brush_.setColor(color);
	if(colorpressure_==false)
		brush_.setColor2(color);
	_needupdate = true;
	update();
}

void BrushPreview::setColor2(const QColor& color)
{
	color2_ = color;
	if(colorpressure_)
		brush_.setColor2(color);
	_needupdate = true;
	update();
}

void BrushPreview::setFloodFillTolerance(int tolerance)
{
	_fillTolerance = tolerance;
	_needupdate = true;
	update();
}

void BrushPreview::setFloodFillExpansion(int expansion)
{
	_fillExpansion = expansion;
	_needupdate = true;
	update();
}

const paintcore::Brush &BrushPreview::brush(bool swapcolors) const
{
	if(swapcolors) {
		swapbrush_ = brush_;
		swapbrush_.setColor(color2_);
		if(colorpressure_)
			swapbrush_.setColor2(color1_);
		else
			swapbrush_.setColor2(color2_);
		return swapbrush_;
	} else {
		return brush_;
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	_needupdate = true;
}

void BrushPreview::changeEvent(QEvent *event)
{
	Q_UNUSED(event);
	_needupdate = true;
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	if(_needupdate)
		updatePreview();
	preview_->paint(event->rect(), &painter);
}

void BrushPreview::updatePreview()
{
	if(preview_==0) {
		preview_ = new paintcore::LayerStack;
		QSize size = contentsRect().size();
		preview_->resize(0, size.width(), size.height(), 0);
		preview_->addLayer(0, "", QColor(0,0,0));
	} else if(preview_->width() != contentsRect().width() || preview_->height() != contentsRect().height()) {
		preview_->resize(0, contentsRect().width() - preview_->width(), contentsRect().height() - preview_->height(), 0);
	}

	QRectF previewRect(
		preview_->width()/8,
		preview_->height()/4,
		preview_->width()-preview_->width()/4,
		preview_->height()-preview_->height()/2
	);
	paintcore::PointVector pointvector;

	switch(shape_) {
	case Stroke: pointvector = paintcore::shapes::sampleStroke(previewRect); break;
	case Line:
		pointvector
			<< paintcore::Point(previewRect.left(), previewRect.top(), 1.0)
			<< paintcore::Point(previewRect.right(), previewRect.bottom(), 1.0);
		break;
	case Rectangle: pointvector = paintcore::shapes::rectangle(previewRect); break;
	case Ellipse: pointvector = paintcore::shapes::ellipse(previewRect); break;
	case FloodFill: pointvector = paintcore::shapes::sampleBlob(previewRect); break;
	}

	paintcore::Layer *layer = preview_->getLayerByIndex(0);
	layer->fillRect(QRect(0, 0, layer->width(), layer->height()), isTransparentBackground() ? QColor(Qt::transparent) : color2_);

	qreal distance = 0;
	for(int i=1;i<pointvector.size();++i)
		layer->drawLine(0, brush_, pointvector[i-1], pointvector[i], distance);

	layer->mergeSublayer(0);

	if(shape_ == FloodFill) {
		paintcore::FillResult fr = paintcore::floodfill(preview_, previewRect.center().toPoint(), color2_, _fillTolerance, 0);
		if(_fillExpansion>0)
			fr = paintcore::expandFill(fr, _fillExpansion, color2_);
		layer->putImage(fr.x, fr.y, fr.image, true);
	}

	_needupdate=false;
}

/**
 * @param brush brush to set
 */
void BrushPreview::setBrush(const paintcore::Brush& brush)
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
	brush_.setSize(size);
	if(sizepressure_==false)
		brush_.setSize2(size);
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
		brush_.setSize2(1);
	else
		brush_.setSize2(brush_.size1());
	updatePreview();
	update();
}

void BrushPreview::setOpacityPressure(bool enable)
{
	opacitypressure_ = enable;
	if(enable)
		brush_.setOpacity2(0);
	else
		brush_.setOpacity2(brush_.opacity1());
	updatePreview();
	update();
}

void BrushPreview::setHardnessPressure(bool enable)
{
	hardnesspressure_ = enable;
	if(enable)
		brush_.setHardness2(0);
	else
		brush_.setHardness2(brush_.hardness1());
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

void BrushPreview::setSubpixel(bool enable)
{
	brush_.setSubpixel(enable);
	updatePreview();
	update();
}

void BrushPreview::setBlendingMode(int mode)
{
	brush_.setBlendingMode(mode);
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
		brush_.setSubpixel(false);
	} else {
		brush_.setHardness(oldhardness1_);
		brush_.setHardness2(oldhardness2_);
		brush_.setSubpixel(true);
	}
	updatePreview();
	update();
}

void BrushPreview::setIncremental(bool incremental)
{
	brush_.setIncremental(incremental);
	updatePreview();
	update();
}

void BrushPreview::contextMenuEvent(QContextMenuEvent *e)
{
	_ctxmenu->popup(e->globalPos());
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestFgColorChange();
}

#ifndef DESIGNER_PLUGIN
}
#endif

