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
#endif

#include <QPaintEvent>
#include <QPainter>
#include <QEvent>
#include <QMenu>

#ifndef DESIGNER_PLUGIN
namespace widgets {
#endif

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f), m_preview(nullptr), m_previewCache(nullptr),
	m_bg(Qt::white),
	m_shape(Stroke), m_fillTolerance(0), m_fillExpansion(0), m_underFill(false),
	m_needupdate(true), m_tranparentbg(false)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);

	m_ctxmenu = new QMenu(this);
	m_ctxmenu->addAction(tr("Change Foreground Color"), this, SIGNAL(requestColorChange()));
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

	// Decide background color
	const QColor c = brush.color();
	const qreal lum = c.redF() * 0.216 + c.greenF() * 0.7152 + c.blueF() * 0.0722;

	if(lum < 0.8) {
		m_bg = Qt::white;
	} else {
		m_bg = QColor(32, 32, 32);
	}

	m_needupdate = true;
	update();
}

void BrushPreview::setPreviewShape(PreviewShape shape)
{
	m_shape = shape;
	m_needupdate = true;
	update();
}

void BrushPreview::setTransparentBackground(bool transparent)
{
	m_tranparentbg = transparent;
	m_needupdate = true;
	update();
}

void BrushPreview::setFloodFillTolerance(int tolerance)
{
	m_fillTolerance = tolerance;
	m_needupdate = true;
	update();
}

void BrushPreview::setFloodFillExpansion(int expansion)
{
	m_fillExpansion = expansion;
	m_needupdate = true;
	update();
}

void BrushPreview::setUnderFill(bool underfill)
{
	m_underFill = underfill;
	m_needupdate = true;
	update();
}

void BrushPreview::resizeEvent(QResizeEvent *)
{ 
	m_needupdate = true;
}

void BrushPreview::changeEvent(QEvent *event)
{
	Q_UNUSED(event)
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

	QRectF previewRect(
		m_preview->width()/8,
		m_preview->height()/4,
		m_preview->width()-m_preview->width()/4,
		m_preview->height()-m_preview->height()/2
	);
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

	QColor bgcolor = m_bg;

	brushes::ClassicBrush brush = m_brush;
	// Special handling for some blending modes
	// TODO this could be implemented in some less ad-hoc way
	if(brush.blendingMode() == paintcore::BlendMode::MODE_BEHIND) {
		// "behind" mode needs a transparent layer for anything to show up
		brush.setBlendingMode(paintcore::BlendMode::MODE_NORMAL);

	} else if(brush.blendingMode() == paintcore::BlendMode::MODE_COLORERASE) {
		// Color-erase mode: use fg color as background
		bgcolor = brushColor();
	}

	if(m_shape == FloodFill) {
		brush.setColor(bgcolor);
	}

	auto layer = layerstack.getEditableLayerByIndex(0);
	layer.putTile(0, 0, 99999, isTransparentBackground() ? paintcore::Tile() : paintcore::Tile(bgcolor));

	brushes::BrushEngine brushengine;
	brushengine.setBrush(1, 1, brush);

	for(int i=0;i<pointvector.size();++i)
		brushengine.strokeTo(pointvector[i], layer.layer());
	brushengine.endStroke();

	const auto dabs = brushengine.takeDabs();
	for(int i=0;i<dabs.size();++i)
		brushes::drawBrushDabsDirect(*dabs.at(i), layer);

	layer.mergeSublayer(1);

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

void BrushPreview::contextMenuEvent(QContextMenuEvent *e)
{
	m_ctxmenu->popup(e->globalPos());
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestColorChange();
}

#ifndef DESIGNER_PLUGIN
}
#endif

