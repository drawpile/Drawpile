// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/widgets/brushpreview.h"
#include "desktop/toolwidgets/fillsettings.h"

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

void BrushPreview::setFloodFillMode(int mode)
{
	if(m_fillMode != mode) {
		m_fillMode = mode;
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
	QRect rect{0, 0, width(), height()};
	painter.drawTiledPixmap(rect, m_background);
#else
	QRect rect = event->rect();
	painter.drawPixmap(rect, m_brushPreview.pixmap(), rect);
#endif
	if(!isEnabled()) {
		QColor color = palette().color(QPalette::Window);
		color.setAlphaF(0.75);
		painter.fillRect(rect, color);
	}
}

void BrushPreview::updatePreview()
{
#ifndef DESIGNER_PLUGIN
	const QSize size = contentsRect().size();
	m_brushPreview.reset(size);

	m_brush.renderPreview(m_brushPreview, m_shape);
	if(m_shape == DP_BRUSH_PREVIEW_FLOOD_FILL || m_shape == DP_BRUSH_PREVIEW_FLOOD_ERASE) {
		m_brushPreview.floodFill(
			m_brush.qColor(), m_fillTolerance / 255.0, m_fillExpansion,
			m_fillFeatherRadius, tools::FillSettings::modeIndexToBlendMode(m_fillMode));
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

