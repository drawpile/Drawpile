/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2020 Calle Laakkonen

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

#include "desktop/widgets/brushpreview.h"

#include <QPaintEvent>
#include <QPainter>
#include <QEvent>

namespace widgets {

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent,f)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32,32);

	const int w = 16;
	m_background = QPixmap(w*2, w*2);
	m_background.fill(QColor(128, 128, 128));
	QPainter p(&m_background);
	p.fillRect(0, 0, w, w, Qt::white);
	p.fillRect(w, w, w, w, Qt::white);
}

BrushPreview::~BrushPreview() {
#ifndef DESIGNER_PLUGIN
	rustpile::brushpreview_free(m_previewcanvas);
#endif
}

void BrushPreview::setBrush(const brushes::ActiveBrush &brush)
{
	m_brush = brush;
	m_needUpdate = true;
	update();
}

void BrushPreview::setPreviewShape(rustpile::BrushPreviewShape shape)
{
	if(m_shape != shape) {
		m_shape = shape;
		m_needUpdate = true;
		update();
	}
}

void BrushPreview::setFloodFillTolerance(int tolerance)
{
	if(m_fillTolerance != tolerance) {
		m_fillTolerance = tolerance;
		m_needUpdate = true;
		update();
	}
}

void BrushPreview::setFloodFillExpansion(int expansion)
{
	if(m_fillExpansion != expansion) {
		m_fillExpansion = expansion;
		m_needUpdate = true;
		update();
	}
}

void BrushPreview::setUnderFill(bool underfill)
{
	if(m_underFill != underfill) {
		m_underFill = underfill;
		m_needUpdate = true;
		update();
	}
}

void BrushPreview::resizeEvent(QResizeEvent *)
{
	m_needUpdate = true;
}

void BrushPreview::changeEvent(QEvent *)
{
	m_needUpdate = true;
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	if(m_needUpdate)
		updatePreview();

	QPainter painter(this);
#ifdef DESIGNER_PLUGIN
	Q_UNUSED(event)
	painter.drawTiledPixmap(0, 0, width(), height(), m_background);
#else
	painter.drawPixmap(event->rect(), m_preview, event->rect());
#endif
}

void BrushPreview::updatePreview()
{
#ifndef DESIGNER_PLUGIN
	const QSize size = contentsRect().size();

	if(size != m_preview.size()) {
		rustpile::brushpreview_free(m_previewcanvas);
		m_previewcanvas = rustpile::brushpreview_new(size.width(), size.height());
		m_preview = QPixmap(size);
	}

	m_brush.renderPreview(m_previewcanvas, m_shape);
	if(m_shape == rustpile::BrushPreviewShape::FloodFill || m_shape == rustpile::BrushPreviewShape::FloodErase) {
		rustpile::Color color = m_brush.color();
		if(m_shape == rustpile::BrushPreviewShape::FloodErase) {
			color.a = 0;
		}
		rustpile::brushpreview_floodfill(m_previewcanvas, &color, m_fillTolerance / 255.0, m_fillExpansion, m_underFill);
	}

	QPainter p(&m_preview);
	p.drawTiledPixmap(0, 0, m_preview.width(), m_preview.height(), m_background);
	rustpile::brushpreview_paint(m_previewcanvas, &p, [](void *p, int x, int y, const uchar *pixels) {
		const QImage img(pixels, 64, 64, 64*4, QImage::Format_ARGB32_Premultiplied);
		static_cast<QPainter*>(p)->drawImage(x, y, img);
	});

#endif
	m_needUpdate=false;
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestColorChange();
}

}

