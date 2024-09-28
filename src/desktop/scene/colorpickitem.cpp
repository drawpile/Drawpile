// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/colorpickitem.h"
#include "desktop/settings.h"
#include "libclient/tools/devicetype.h"
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

bool ColorPickItem::shouldShow(int source, int visibility, const QColor &color)
{
	if(color.isValid() && color.alpha() > 0) {
		switch(visibility) {
		case int(desktop::settings::SamplingRingVisibility::Never):
			return false;
		case int(desktop::settings::SamplingRingVisibility::TouchOnly):
			return source == int(tools::ColorPickSource::Touch);
		default:
			return true;
		}
	} else {
		return false;
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
		cachePainter.setPen(pen);

		QRectF cacheRect = m_cache.rect();
		QRectF outerRect = cacheRect.marginsRemoved(
			QMarginsF(penWidth, penWidth, penWidth, penWidth));
		cachePainter.setBrush(m_color);
		cachePainter.setClipRect(
			QRectF(0, 0, cacheRect.width(), cacheRect.height() / 2.0));
		cachePainter.drawEllipse(outerRect);

		cachePainter.setBrush(m_comparisonColor);
		cachePainter.setClipRect(QRectF(
			0, cacheRect.height() / 2.0, cacheRect.width(),
			cacheRect.height() / 2.0));
		cachePainter.drawEllipse(outerRect);
		cachePainter.setClipRect(QRectF(), Qt::NoClip);

		qreal innerX = cacheRect.width() / 8.0;
		qreal innerY = cacheRect.height() / 8.0;
		QRectF innerRect =
			cacheRect.marginsRemoved(QMarginsF(innerX, innerY, innerX, innerY));

		cachePainter.setPen(Qt::NoPen);
		cachePainter.setCompositionMode(QPainter::CompositionMode_Clear);
		cachePainter.drawEllipse(innerRect);

		cachePainter.setBrush(Qt::transparent);
		cachePainter.setPen(pen);
		cachePainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
		cachePainter.drawEllipse(innerRect);
	}
	painter->drawPixmap(rect, m_cache);
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
