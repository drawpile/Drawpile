// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/colorpickitem.h"
#include "desktop/settings.h"
#include "libclient/tools/enums.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QtMath>

namespace drawingboard {

ColorPickItem::ColorPickItem(
	const QColor &color, const QColor &comparisonColor, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_color(sanitizeColor(color))
	, m_comparisonColor(sanitizeColor(comparisonColor))
{
	setFlag(ItemIgnoresTransformations);
	setZValue(Z_COLORPICK);
}

QRectF ColorPickItem::boundingRect() const
{
	return QRectF(bounds());
}

void ColorPickItem::setColor(const QColor &color)
{
	QColor c = sanitizeColor(color);
	if(c != m_color) {
		m_color = c;
		m_cache = QPixmap();
		refresh();
	}
}

void ColorPickItem::setComparisonColor(const QColor &comparisonColor)
{
	QColor c = sanitizeColor(comparisonColor);
	if(comparisonColor != m_comparisonColor) {
		m_comparisonColor = c;
		m_cache = QPixmap();
		refresh();
	}
}

bool ColorPickItem::shouldShow(int source, int visibility)
{
	if(source == int(tools::ColorPickSource::Adjust)) {
		return true;
	} else {
		switch(visibility) {
		case int(desktop::settings::SamplingRingVisibility::Never):
			return false;
		case int(desktop::settings::SamplingRingVisibility::TouchOnly):
			return source == int(tools::ColorPickSource::Touch);
		default:
			return true;
		}
	}
}

int ColorPickItem::defaultVisibility()
{
	return int(desktop::settings::SamplingRingVisibility::Always);
}

void ColorPickItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QRect rect = bounds();
	qreal dpr = painter->device()->devicePixelRatioF();
	QSizeF cacheSizeF = QRectF(rect).size() * dpr;
	QSize cacheSize(qCeil(cacheSizeF.width()), qCeil(cacheSizeF.height()));
	if(m_cache.isNull() || m_cache.size() != cacheSize) {
		m_cache = QPixmap(cacheSize);
		m_cache.fill(Qt::transparent);
		QPainter cachePainter(&m_cache);
		cachePainter.setRenderHint(QPainter::Antialiasing);

		QColor baseColor = qApp->palette().color(QPalette::Base);
		baseColor.setAlpha(128);
		qreal penWidth = 2.0 * dpr;
		QPen pen = QPen(baseColor, penWidth);

		QRectF cacheRect = m_cache.rect();
		const QPainterPath &path = getPath(penWidth, cacheRect);

		if(m_color == m_comparisonColor) {
			cachePainter.setPen(pen);
			cachePainter.setBrush(m_color);
			cachePainter.drawPath(path);
		} else {
			cachePainter.setPen(Qt::NoPen);

			cachePainter.setBrush(m_color);
			cachePainter.drawPath(path);
			cachePainter.setClipRect(QRectF(
				0, 0, cacheRect.width(), cacheRect.height() / 2.0 + 1.0));
			cachePainter.drawPath(path);

			cachePainter.setBrush(m_comparisonColor);
			cachePainter.setClipRect(QRectF(
				0, cacheRect.height() / 2.0, cacheRect.width(),
				cacheRect.height() / 2.0));
			cachePainter.drawPath(path);

			cachePainter.setPen(pen);
			cachePainter.setBrush(Qt::NoBrush);
			cachePainter.setClipRect(QRectF(), Qt::NoClip);
			cachePainter.drawPath(path);
		}
	}
	painter->drawPixmap(rect, m_cache);
}

const QPainterPath &
ColorPickItem::getPath(qreal penWidth, const QRectF &cacheRect)
{
	bool needsNewPath = m_path.isEmpty() || penWidth != m_pathPenWidth ||
						cacheRect != m_pathCacheRect;
	if(needsNewPath) {
		m_pathPenWidth = penWidth;
		m_pathCacheRect = cacheRect;
		m_path.clear();

		QRectF outerRect = cacheRect.marginsRemoved(
			QMarginsF(penWidth, penWidth, penWidth, penWidth));

		qreal innerX = cacheRect.width() / 8.0;
		qreal innerY = cacheRect.height() / 8.0;
		QRectF innerRect =
			cacheRect.marginsRemoved(QMarginsF(innerX, innerY, innerX, innerY));

		QPointF center = cacheRect.center();
		qreal focusLeft = outerRect.left();
		qreal focusRight = outerRect.right();
		qreal focusWidth = innerX * 1.75;
		qreal focusOffsetY = innerY * 1.5;
		qreal focusTop = center.y() - focusOffsetY;
		qreal focusBottom = center.y() + focusOffsetY;
		QRectF leftFocusRect(
			QPointF(focusLeft, focusTop),
			QPointF(focusLeft + focusWidth, focusBottom));
		QRectF rightFocusRect(
			QPointF(focusRight - focusWidth, focusTop),
			QPointF(focusRight, focusBottom));

		QPainterPath focusPath;
		focusPath.addEllipse(leftFocusRect);
		focusPath.addEllipse(rightFocusRect);

		QPainterPath innerPath;
		innerPath.addEllipse(innerRect);

		QPainterPath path;
		path.addEllipse(outerRect);
		m_path = path.subtracted(innerPath.subtracted(focusPath)).simplified();
	}
	return m_path;
}

QColor ColorPickItem::sanitizeColor(const QColor &color)
{
	if(color.isValid() && color.alpha() > 0) {
		QColor c = color;
		c.setAlpha(255);
		return c;
	} else {
		return Qt::black;
	}
}

}
