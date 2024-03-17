// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/annotationitem.h"
#include "libclient/utils/annotations.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QTextBlock>

namespace drawingboard {

AnnotationItem::AnnotationItem(int id, QGraphicsItem *parent)
	: QGraphicsItem(parent)
	, m_id(id)
{
}

void AnnotationItem::setGeometry(const QRect &rect)
{
	if(m_rect != rect) {
		prepareGeometryChange();
		m_rect = rect;
		m_doc.setTextWidth(rect.width());
	}
}

void AnnotationItem::setColor(const QColor &color)
{
	if(m_color != color) {
		m_color = color;
		update();
	}
}

void AnnotationItem::setText(const QString &text)
{
	m_doc.setHtml(text);
	update();
}

void AnnotationItem::setValign(int valign)
{
	if(m_valign != valign) {
		m_valign = valign;
		update();
	}
}

/**
 * Highlight is used to indicate the selected annotation.
 * @param hl
 */
void AnnotationItem::setHighlight(bool hl)
{
	if(m_highlight != hl) {
		m_highlight = hl;
		update();
	}
}

/**
 * Border is normally drawn when the annotation is highlighted or has no text.
 * The border is forced on when the annotation edit tool is selected.
 */
void AnnotationItem::setShowBorder(bool show)
{
	if(m_showborder != show) {
		m_showborder = show;
		update();
	}
}

QRectF AnnotationItem::boundingRect() const
{
	return m_rect.adjusted(-HANDLE / 2, -HANDLE / 2, HANDLE / 2, HANDLE / 2);
}

void AnnotationItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	painter->save();
	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	painter->fillRect(m_rect, m_color);

	paintHiddenBorder(painter);

	if(m_highlight) {
		// Draw resizing handles
		QColor border = QApplication::palette().color(QPalette::Highlight);
		border.setAlpha(255);

		QPen pen(border);
		pen.setCosmetic(true);
		pen.setWidth(HANDLE);
		painter->setPen(pen);
		painter->drawPoint(m_rect.topLeft());
		painter->drawPoint(m_rect.topLeft() + QPointF(m_rect.width() / 2, 0));
		painter->drawPoint(m_rect.topRight());

		painter->drawPoint(m_rect.topLeft() + QPointF(0, m_rect.height() / 2));
		painter->drawPoint(m_rect.topRight() + QPointF(0, m_rect.height() / 2));

		painter->drawPoint(m_rect.bottomLeft());
		painter->drawPoint(
			m_rect.bottomLeft() + QPointF(m_rect.width() / 2, 0));
		painter->drawPoint(m_rect.bottomRight());
	}

	m_doc.setTextWidth(m_rect.width());

	QPointF offset;
	switch(m_valign) {
	case 1:
		offset.setY((m_rect.height() - m_doc.size().height()) / 2);
		break;
	case 2:
		offset.setY(m_rect.height() - m_doc.size().height());
		break;
	}

	painter->translate(m_rect.topLeft() + offset);
	m_doc.drawContents(painter, QRectF(-offset, m_rect.size()));

	painter->restore();
}

void AnnotationItem::paintHiddenBorder(QPainter *painter)
{
	// Hidden border is usually hidden
	if(!m_showborder && !m_doc.isEmpty())
		return;

	QColor highlightColor = QApplication::palette().color(QPalette::Highlight);
	highlightColor.setAlpha(255);

	QPen bpen(m_highlight && m_showborder ? Qt::DashLine : Qt::DotLine);
	bpen.setWidth(painter->device()->devicePixelRatioF());
	bpen.setCosmetic(true);
	bpen.setColor(highlightColor);
	painter->setPen(bpen);
	painter->drawRect(m_rect);
}

QImage AnnotationItem::toImage() const
{
	QImage img(m_rect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
	img.fill(0);
	QPainter painter(&img);
	utils::paintAnnotation(
		&painter, m_rect.size().toSize(), m_color, text(), m_valign);
	return img;
}

}
