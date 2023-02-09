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

#include "brushpreview.h"

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

BrushPreview::~BrushPreview()
{
}

void BrushPreview::setBrush(const brushes::ActiveBrush &brush)
{
	m_brush = brush;
	m_needUpdate = true;
	update();
}

void BrushPreview::setPreviewShape(DP_BrushPreviewShape shape)
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

void BrushPreview::setFloodFillFeatherRadius(int featherRadius)
{
	if(m_fillFeatherRadius != featherRadius) {
		m_fillFeatherRadius = featherRadius;
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
	painter.drawPixmap(event->rect(), m_brushPreview.pixmap(), event->rect());
#endif
}

void BrushPreview::updatePreview()
{
#ifndef DESIGNER_PLUGIN
	const QSize size = contentsRect().size();
	m_brushPreview.reset(size);

	m_brush.renderPreview(m_brushPreview, m_shape);
	if(m_shape == DP_BRUSH_PREVIEW_FLOOD_FILL || m_shape == DP_BRUSH_PREVIEW_FLOOD_ERASE) {
		QColor color = m_brush.qColor();
		if(m_shape == DP_BRUSH_PREVIEW_FLOOD_ERASE) {
			color.setAlpha(0);
		}
		m_brushPreview.floodFill(
			color, m_fillTolerance / 255.0, m_fillExpansion, m_fillFeatherRadius, m_underFill);
	}

	m_brushPreview.paint(m_background);
#endif
	m_needUpdate = false;
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent*)
{
	emit requestColorChange();
}

}

