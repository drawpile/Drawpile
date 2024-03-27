// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/catchupitem.h"
#include <QApplication>
#include <QColor>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

CatchupItem::CatchupItem(const QString &text, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_text(text)
	, m_percent(0)
	, m_fade(1.0)
{
	setFlag(ItemIgnoresTransformations);
	setZValue(9999);
	updateBarText();
}

QRectF CatchupItem::boundingRect() const
{
	return m_bounds;
}

void CatchupItem::setCatchupProgress(int percent)
{
	m_percent = percent;
	if(percent < 100) {
		m_fade = 1.0;
	}
	updateBarText();
	refresh();
}

bool CatchupItem::animationStep(qreal dt)
{
	if(m_percent < 100) {
		return true;
	} else {
		m_fade = qMax(0.0, m_fade - dt);
		refresh();
		return m_fade > 0.0;
	}
}

void CatchupItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);

	if(m_fade < 1.0) {
		painter->setOpacity(m_fade);
	}

	QPalette palette = qApp->palette();
	painter->setPen(Qt::NoPen);
	QColor backgroundColor = palette.base().color();
	painter->setBrush(backgroundColor);
	painter->drawRect(m_bounds);

	painter->setFont(qApp->font());
	QColor textColor = palette.text().color();
	painter->setPen(textColor);
	painter->setBrush(Qt::NoBrush);
	painter->drawText(m_textBounds, Qt::AlignCenter, m_text);
	bool partial = m_percent < 100;
	if(partial) {
		painter->drawText(m_barBounds, Qt::AlignCenter, m_barText);
	}

	QPen pen = painter->pen();
	pen.setWidthF(2.0);
	painter->setPen(pen);
	if(partial) {
		painter->drawRect(m_barBounds);
		QRectF bar = m_barBounds;
		bar.setWidth(bar.width() * qreal(m_percent) / 100.0);
		painter->fillRect(bar, textColor);
		painter->setClipRect(bar);
	} else {
		painter->setBrush(textColor);
		painter->drawRect(m_barBounds);
	}

	painter->setPen(backgroundColor);
	painter->drawText(m_barBounds, Qt::AlignCenter, m_barText);
}

void CatchupItem::updateBarText()
{
	m_barText = QStringLiteral("%1%").arg(m_percent);
	updateBounds();
}

void CatchupItem::updateBounds()
{
	refreshGeometry();
	QFontMetricsF fontMetrics(qApp->font());
	qreal margin = qRound(fontMetrics.height());
	QRect fontRect(0, 0, 0xffff, 0xffff);
	QRectF textRect = fontMetrics.boundingRect(fontRect, 0, m_text)
						  .translated(margin, margin);
	QRectF barTextRect =
		QFontMetricsF(qApp->font())
			.boundingRect(fontRect, 0, m_barText)
			.translated(margin, textRect.bottom() + margin / 2.0);
	qreal width = qMax(textRect.width(), margin * 20.0);
	qreal height = barTextRect.height() * 1.5;
	m_textBounds = QRectF(textRect.x(), textRect.y(), width, textRect.height());
	m_barBounds = QRectF(barTextRect.x(), barTextRect.y(), width, height);
	m_bounds =
		m_textBounds.united(m_barBounds)
			.marginsAdded(QMargins(margin, margin, margin, margin * 1.2));
}

}
