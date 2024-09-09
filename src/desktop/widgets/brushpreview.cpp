// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/widgets/brushpreview.h"
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>

namespace widgets {

BrushPreview::BrushPreview(QWidget *parent, Qt::WindowFlags f)
	: QFrame(parent, f)
{
	setAttribute(Qt::WA_NoSystemBackground);
	setMinimumSize(32, 32);
	updateBackground();
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

void BrushPreview::resizeEvent(QResizeEvent *)
{
	m_needUpdate = true;
}

void BrushPreview::changeEvent(QEvent *event)
{
	if(event->type() == QEvent::PaletteChange) {
		updateBackground();
	}
	m_needUpdate = true;
	update();
}

void BrushPreview::paintEvent(QPaintEvent *event)
{
	qreal dpr = devicePixelRatioF();
	if(m_needUpdate || m_lastDpr != dpr) {
		updatePreview(dpr);
	}

	QPainter painter(this);
	QRect rect = event->rect();
	painter.drawPixmap(
		rect, m_brushPreview.pixmap(),
		QRect(
			rect.x() * dpr, rect.y() * dpr, rect.width() * dpr,
			rect.height() * dpr));

	if(!isEnabled()) {
		QColor color = palette().color(QPalette::Window);
		color.setAlphaF(0.75);
		painter.fillRect(rect, color);
	}
}

void BrushPreview::updateBackground()
{
	int w = 16;
	bool dark = palette().color(QPalette::Window).lightness() < 128;
	m_background = QPixmap(w * 2, w * 2);
	m_background.fill(dark ? QColor(153, 153, 153) : QColor(204, 204, 204));
	QColor alt = dark ? QColor(102, 102, 102) : Qt::white;
	QPainter p(&m_background);
	p.fillRect(0, 0, w, w, alt);
	p.fillRect(w, w, w, w, alt);
}

void BrushPreview::updatePreview(qreal dpr)
{
	const QSize size = contentsRect().size() * dpr;
	m_brushPreview.reset(size);
	m_brush.renderPreview(m_brushPreview, m_shape);
	m_brushPreview.paint(m_background);
	m_needUpdate = false;
	m_lastDpr = dpr;
}

void BrushPreview::mouseDoubleClickEvent(QMouseEvent *)
{
	emit requestColorChange();
}

}
