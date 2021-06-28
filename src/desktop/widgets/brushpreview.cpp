/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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

#include "brushpreview.h"

#ifndef DESIGNER_PLUGIN
#include "core/point.h"
#include "core/layerstack.h"
#include "core/layerstackpixmapcacheobserver.h"
#include "core/layer.h"
#include "core/floodfill.h"
#include "brushes/shapes.h"
#include "brushes/brushengine.h"
#include "brushes/brushpainter.h"
#include "utils/icon.h"
#endif

#include <QPaintEvent>
#include <QPainter>
#include <QEvent>

namespace widgets {

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);
}

BrushPreview::~BrushPreview() {
#ifndef DESIGNER_PLUGIN
	delete m_previewCache;
	delete m_preview;
#endif
}

void BrushPreview::setBrush(const brushes::ClassicBrush &brush)
{
	m_brush = brush;
	m_needupdate = true;
	update();
}

void BrushPreview::setPreviewShape(PreviewShape shape)
{
	if(m_shape != shape) {
		m_shape = shape;
		m_needupdate = true;
		update();
	}
}

void BrushPreview::setFloodFillTolerance(int tolerance)
{
	if(m_fillTolerance != tolerance) {
		m_fillTolerance = tolerance;
		m_needupdate = true;
		update();
	}
}

void BrushPreview::setFloodFillExpansion(int expansion)
{
	if(m_fillExpansion != expansion) {
		m_fillExpansion = expansion;
		m_needupdate = true;
		update();
	}
}

void BrushPreview::setUnderFill(bool underfill)
{
	if(m_underFill != underfill) {
		m_underFill = underfill;
		m_needupdate = true;
		update();
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	m_needupdate = true;
}

void BrushPreview::changeEvent(QEvent *)
{
	m_needupdate = true;
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
#ifndef DESIGNER_PLUGIN
	if(m_needupdate)
		updatePreview();

	QPainter painter(this);
	painter.drawPixmap(event->rect(), m_previewCache->getPixmap(event->rect()), event->rect());
#endif
}

void BrushPreview::updatePreview()
{
#ifndef DESIGNER_PLUGIN
	if(!m_preview) {
		m_preview = new paintcore::LayerStack;
		m_previewCache = new paintcore::LayerStackPixmapCacheObserver(this);
		m_previewCache->attachToLayerStack(m_preview);
	}

	auto layerstack = m_preview->editor(0);

	if(m_preview->width() == 0) {
		const QSize size = contentsRect().size();
		layerstack.resize(0, size.width(), size.height(), 0);
		layerstack.createLayer(0, 0, QColor(0,0,0), false, false, QString());

	} else if(m_preview->width() != contentsRect().width() || m_preview->height() != contentsRect().height()) {
		layerstack.resize(0, contentsRect().width() - m_preview->width(), contentsRect().height() - m_preview->height(), 0);
	}

	const QRectF previewRect {
		m_preview->width()/8.0,
		m_preview->height()/4.0,
		m_preview->width()-m_preview->width()/4.0,
		m_preview->height()-m_preview->height()/2.0
	};

	paintcore::PointVector pointvector;

	switch(m_shape) {
	case Stroke: pointvector = brushes::shapes::sampleStroke(previewRect); break;
	case Line:
		pointvector
			<< paintcore::Point(previewRect.left(), previewRect.top(), 1.0)
			<< paintcore::Point(previewRect.right(), previewRect.bottom(), 1.0);
		break;
	case Rectangle: pointvector = brushes::shapes::rectangle(previewRect); break;
	case Ellipse: pointvector = brushes::shapes::ellipse(previewRect); break;
	case FloodFill:
	case FloodErase: pointvector = brushes::shapes::sampleBlob(previewRect); break;
	}

	QColor bgColor = icon::isDark(m_brush.color()) ? QColor(250, 250, 250) : QColor(32, 32, 32);
	QColor layerColor = Qt::transparent;
	enum class LayerFill {
		Solid,
		RainbowBars,
		RainbowDabs
	};
	auto fgStyle = m_brush.smudge1()>0 ? LayerFill::RainbowBars : LayerFill::Solid;

	constexpr int DAB_HUE = 0x1;
	constexpr int DAB_SATURATION = 0x2;
	constexpr int DAB_VALUE = 0x4;
	int dabModes = 0x0;

	brushes::ClassicBrush brush = m_brush;

	if(brush.blendingMode() == paintcore::BlendMode::MODE_ERASE) {
		layerColor = bgColor;
		bgColor = Qt::transparent;

	} else if(brush.blendingMode() == paintcore::BlendMode::MODE_COLORERASE) {
		// Color-erase mode: use fg color as background
		bgColor = Qt::transparent;
		layerColor = brushColor();

	} else if(!paintcore::findBlendMode(brush.blendingMode()).flags.testFlag(paintcore::BlendMode::IncrOpacity)) {
		fgStyle = LayerFill::RainbowDabs;
		bgColor = Qt::transparent;
		switch(brush.blendingMode()) {
		case paintcore::BlendMode::MODE_HSL_HUE:
		case paintcore::BlendMode::MODE_HSL_SATURATION:
		case paintcore::BlendMode::MODE_HSL_LUMINOSITY:
		case paintcore::BlendMode::MODE_HSL_COLOR:
			dabModes = DAB_HUE | DAB_SATURATION | DAB_VALUE;
			break;
		default:
			dabModes = DAB_HUE;
			break;
		}
	}

	if(m_shape == FloodFill) {
		brush.setColor(bgColor);
		bgColor = Qt::transparent;

	} else if(m_shape == FloodErase) {
		layerColor = brush.color();
		brush.setColor(bgColor);
		bgColor = Qt::transparent;
	}

	auto layer = layerstack.getEditableLayerByIndex(0);
	layerstack.setBackground(paintcore::Tile(bgColor));
	layer.putTile(0, 0, 99999, paintcore::Tile(layerColor));

	if(fgStyle == LayerFill::Solid) {

		// When using the "behind" mode, draw something in the foreground to show off the effect
		if(brush.blendingMode() == paintcore::BlendMode::MODE_BEHIND) {
			const int w = layer->width();
			const int h = layer->height();
			const int b = qMax(2, w / 20);
			layer.fillRect(QRect{w/4*1-b/2, 0, b, h}, Qt::black, paintcore::BlendMode::MODE_NORMAL);
			layer.fillRect(QRect{w/4*2-b/2, 0, b, h}, Qt::gray, paintcore::BlendMode::MODE_NORMAL);
			layer.fillRect(QRect{w/4*3-b/2, 0, b, h}, Qt::white, paintcore::BlendMode::MODE_NORMAL);
		}

	} else if(fgStyle == LayerFill::RainbowDabs) {
		const int w = layer->width();
		const int h = layer->height();
		const uint8_t d = qBound(10, h*2/3, 255);
		const int x0 = d, x1 = w - d;
		const int step = d * 70 / 100;
		const int huestep = dabModes & DAB_HUE ? 359 / ((x1-x0) / step) : 0;
		const int saturationstep = dabModes & DAB_SATURATION ?  255 / ((x1-x0) / step) : 0;
		const int valuestep = dabModes & DAB_VALUE ? 255 / ((x1-x0) / step) : 0;
		int hue = huestep == 0 ? 359 / 2 : 0;
		int saturation = saturationstep == 0 ? 160 : 0;
		int value = valuestep == 0 ? 220 : 0;
		for(int x=x0;x<x1;x+=step, hue+=huestep, saturation += saturationstep, value += valuestep) {
			protocol::DrawDabsPixel dab(
				protocol::DabShape::Round,
				1,
				layer->id(),
				x, h/2,
				QColor::fromHsv(hue, saturation, value).rgb() & 0x00ffffff,
				paintcore::BlendMode::MODE_NORMAL,
				protocol::PixelBrushDabVector { protocol::PixelBrushDab {0, 0, d, 255} }
			);
			brushes::drawBrushDabsDirect(dab, layer);
		}

	} else if(fgStyle == LayerFill::RainbowBars) {
		const int w = layer->width();
		const int h = layer->height();
		const int bars = 5;
		const int bw = w/bars;

		for(int i=0;i<bars;++i) {
			layer.fillRect(QRect{i*bw, 0, bw, h}, QColor::fromHsv(i*(359/bars), 160, 220), paintcore::BlendMode::MODE_NORMAL);
		}
	}

	// Draw the brush preview shape
	{
		brushes::BrushEngine brushengine;
		brushengine.setBrush(1, 1, brush);

		for(int i=0;i<pointvector.size();++i)
			brushengine.strokeTo(pointvector[i], layer.layer());
		brushengine.endStroke();

		const auto dabs = brushengine.takeDabs();
		for(int i=0;i<dabs.size();++i)
			brushes::drawBrushDabsDirect(*dabs.at(i), layer);

		layer.mergeSublayer(1);
	}

	// Do the flood fill
	// In flood fill mode, the shape drawn with the brush creates
	// a closed area that will be filled here.
	if(m_shape == FloodFill || m_shape == FloodErase) {
		paintcore::FillResult fr = paintcore::floodfill(
			m_preview,
			previewRect.center().toPoint(),
			m_shape == FloodFill ? brushColor() : QColor(),
			m_fillTolerance,
			0,
			false,
			360000);

		if(m_fillExpansion>0)
			fr = paintcore::expandFill(fr, m_fillExpansion, brushColor());
		if(!fr.image.isNull())
			layer.putImage(fr.x, fr.y, fr.image, m_shape == FloodFill ? (m_underFill ? paintcore::BlendMode::MODE_BEHIND : paintcore::BlendMode::MODE_NORMAL) : paintcore::BlendMode::MODE_ERASE);
	}

	m_needupdate=false;
#endif
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestColorChange();
}

}

