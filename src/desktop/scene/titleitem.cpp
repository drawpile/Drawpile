// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/titleitem.h"
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

TitleItem::TitleItem(
	const QString &text, const QColor &color, QGraphicsItem *parent)
	: BaseItem(parent)
{
	setFlag(ItemIgnoresTransformations);
	setZValue(Z_CURSOR);
	setText(text);
	setColor(color);
}

QRectF TitleItem::boundingRect() const
{
	return m_bounds;
}

bool TitleItem::setText(const QString &text)
{
	if(text != m_text) {
		m_text = text;
		updateBounds();
		return true;
	} else {
		return false;
	}
}

void TitleItem::setColor(const QColor &color)
{
	if(color != m_color) {
		if(color.isValid() && color.alpha() != 0) {
			m_color = color;
			m_color.setAlpha(255);
		} else {
			m_color = QColor();
		}
		refresh();
	}
}

void TitleItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);

	QBrush backgroundBrush;
	QPen textPen;
	if(m_color.isValid()) {
		backgroundBrush = QBrush(m_color);
		textPen = QPen(m_color.lightnessF() < 0.7 ? Qt::white : Qt::black);
	} else {
		QPalette pal = qApp->palette();
		backgroundBrush = pal.base();
		textPen = QPen(pal.text(), 1.0);
	}

	painter->setPen(Qt::NoPen);
	painter->setBrush(backgroundBrush);
	painter->drawRect(m_bounds);

	painter->setFont(qApp->font());
	painter->setPen(textPen);
	painter->setBrush(Qt::NoBrush);
	painter->drawText(m_textBounds, Qt::AlignCenter, m_text);
}

void TitleItem::updateBounds()
{
	refreshGeometry();
	m_textBounds = QFontMetricsF(qApp->font())
					   .boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_text)
					   .translated(8, 8);
	m_bounds = m_textBounds.marginsAdded(QMargins{8, 8, 8, 8});
}

}
