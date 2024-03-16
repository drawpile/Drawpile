// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/annotationitem.h"
#include "libclient/utils/annotations.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QTextBlock>

namespace drawingboard {

AnnotationItem::AnnotationItem(int id, qreal zoom, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_id(id)
	, m_handleSize(calculateHandleSize(zoom))
{
}

void AnnotationItem::setGeometry(const QRect &rect)
{
	if(m_rect != rect) {
		refreshGeometry();
		m_rect = rect;
		m_doc.setTextWidth(rect.width());
		m_pixmapDirty = true;
	}
}

void AnnotationItem::setColor(const QColor &color)
{
	if(m_color != color) {
		m_color = color;
		m_pixmapDirty = true;
		refresh();
	}
}

void AnnotationItem::setText(const QString &text)
{
	m_doc.setHtml(text);
	m_aliasDirty = true;
	m_pixmapDirty = true;
	refresh();
}

void AnnotationItem::setValign(int valign)
{
	if(m_valign != valign) {
		m_valign = valign;
		m_pixmapDirty = true;
		refresh();
	}
}

void AnnotationItem::setAlias(bool alias)
{
	if(m_alias != alias) {
		m_alias = alias;
		m_aliasDirty = true;
		m_pixmapDirty = true;
		refresh();
	}
}

void AnnotationItem::setRasterize(bool rasterize)
{
	if(m_rasterize != rasterize) {
		m_rasterize = rasterize;
		m_pixmapDirty = true;
		m_pixmap = QPixmap();
		refresh();
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
		refresh();
	}
}

void AnnotationItem::setZoom(qreal zoom)
{
	int handleSize = calculateHandleSize(zoom);
	if(handleSize != m_handleSize) {
		refreshGeometry();
		m_handleSize = handleSize;
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
		m_pixmapDirty = true;
		refresh();
	}
}

QRectF AnnotationItem::boundingRect() const
{
	qreal h = m_handleSize / 2.0;
	return m_rect.adjusted(-h, -h, h, h);
}

void AnnotationItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);

	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	painter->save();
	if(m_rasterize) {
		painter->setRenderHint(QPainter::Antialiasing, false);
		painter->drawPixmap(m_rect.topLeft(), renderPixmap());
	} else {
		painter->fillRect(m_rect, m_color);
		utils::paintAnnotationContent(
			painter, m_doc, m_alias, m_valign, m_rect, m_aliasDirty);
		m_aliasDirty = false;
	}
	painter->restore();

	paintHiddenBorder(painter);

	if(m_highlight) {
		// Draw resizing handles
		QColor border = QApplication::palette().color(QPalette::Highlight);
		border.setAlpha(255);

		QPen pen(border);
		pen.setCosmetic(true);
		pen.setWidth(HANDLE_SIZE);
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
}

int AnnotationItem::calculateHandleSize(qreal zoom)
{
	return zoom > 0.0 ? qRound(HANDLE_SIZE / zoom) : 0;
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

const QPixmap &AnnotationItem::renderPixmap()
{
	if(m_pixmapDirty) {
		m_pixmapDirty = false;
		m_pixmap = QPixmap(m_rect.toRect().size());
		m_pixmap.fill(m_color);
		QPainter pixmapPainter(&m_pixmap);
		utils::paintAnnotationContent(
			&pixmapPainter, m_doc, m_alias, m_valign,
			QRectF(QPointF(0.0, 0.0), m_rect.size()), m_aliasDirty);
		m_aliasDirty = false;
	}
	return m_pixmap;
}

QImage AnnotationItem::toImage() const
{
	if(m_rasterize) {
		return m_pixmap.toImage().convertToFormat(
			QImage::Format_ARGB32_Premultiplied);
	} else {
		QImage img(m_rect.size().toSize(), QImage::Format_ARGB32_Premultiplied);
		img.fill(0);
		QPainter painter(&img);
		utils::paintAnnotation(
			&painter, m_rect.size().toSize(), m_color, text(), m_alias,
			m_valign);
		return img;
	}
}

}
