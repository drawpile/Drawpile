// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/noticeitem.h"
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QGraphicsScene>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

NoticeItem::NoticeItem(
	const QString &text, qreal persist, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_persist(persist)
{
	setFlag(ItemIgnoresTransformations);
	setZValue(Z_NOTICE);
	setText(text);
}

QRectF NoticeItem::boundingRect() const
{
	return m_bounds;
}

bool NoticeItem::setText(const QString &text)
{
	if(text != m_text) {
		m_text = text;
		updateBounds();
		return true;
	} else {
		return false;
	}
}

bool NoticeItem::setPersist(qreal seconds)
{
	bool changed =
		seconds != m_persist && (seconds < FADEOUT || m_persist < FADEOUT);
	m_persist = seconds;
	setOpacity(m_persist < 0.0 ? 1.0 : qBound(0.0, m_persist / FADEOUT, 1.0));
	if(changed) {
		refresh();
	}
	return changed;
}

bool NoticeItem::setOpacity(qreal opacity)
{
	if(opacity != m_opacity) {
		m_opacity = opacity;
		refresh();
		return true;
	} else {
		return false;
	}
}

bool NoticeItem::animationStep(qreal dt)
{
	if(m_persist < 0.0) {
		return true;
	} else {
		m_persist = qMax(0.0, m_persist - dt);
		if(m_persist >= 0.0 && m_persist < FADEOUT) {
			setOpacity(m_persist / FADEOUT);
			refresh();
		}
		return m_persist > 0.0;
	}
}

void NoticeItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);

	QPalette palette = qApp->palette();
	painter->setPen(Qt::NoPen);
	painter->setBrush(palette.base());
	if(m_opacity < 1.0) {
		painter->setOpacity(m_opacity);
	}
	painter->drawRect(m_bounds);

	painter->setFont(qApp->font());
	painter->setPen(palette.text().color());
	painter->setBrush(Qt::NoBrush);
	if(m_opacity < 1.0) {
		painter->setOpacity(1.0);
	}
	painter->drawText(m_textBounds, Qt::AlignLeft | Qt::AlignVCenter, m_text);
}

void NoticeItem::updateBounds()
{
	refreshGeometry();
	m_textBounds = QFontMetricsF(qApp->font())
					   .boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_text)
					   .translated(16, 16);
	m_bounds = m_textBounds.marginsAdded(QMargins{16, 16, 16, 16});
}

}
