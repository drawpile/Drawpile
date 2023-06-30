// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/noticeitem.h"
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsDropShadowEffect>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

NoticeItem::NoticeItem(const QString &text, QGraphicsItem *parent)
	: QGraphicsItem{parent}
	, m_persist{-1.0}
{
	setFlag(ItemIgnoresTransformations);
	QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect;
	shadow->setOffset(0);
	shadow->setBlurRadius(10);
	setGraphicsEffect(shadow);
	setZValue(9999);
	setText(text);
}

QRectF NoticeItem::boundingRect() const
{
	return m_bounds;
}

void NoticeItem::setText(const QString &text)
{
	m_text = text;
	updateBounds();
}

void NoticeItem::setPersist(qreal seconds)
{
	m_persist = seconds;
	animationStep(0.0);
}

bool NoticeItem::animationStep(qreal dt)
{
	if(m_persist < 0.0) {
		return true;
	} else {
		m_persist = qMax(0.0, m_persist - dt);
		if(m_persist < FADEOUT) {
			update();
		}
		return m_persist > 0.0;
	}
}

void NoticeItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);

	if(m_persist >= 0.0 && m_persist < FADEOUT) {
		painter->setOpacity(m_persist / FADEOUT);
	}

	QPalette palette = qApp->palette();
	painter->setPen(Qt::NoPen);
	painter->setBrush(palette.base());
	painter->drawRect(m_bounds);

	painter->setFont(qApp->font());
	painter->setPen(palette.text().color());
	painter->setBrush(Qt::NoBrush);
	painter->drawText(m_textBounds, Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void NoticeItem::updateBounds()
{
	prepareGeometryChange();
	m_textBounds = QFontMetricsF(qApp->font())
					   .boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_text)
					   .translated(16, 16);
	m_bounds = m_textBounds.marginsAdded(QMargins{16, 16, 16, 16});
}

}
