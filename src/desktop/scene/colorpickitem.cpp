// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/colorpickitem.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>
#include <QtMath>

namespace drawingboard {

ColorPickItem::ColorPickItem(
	const QColor &color, const QColor &comparisonColor, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_renderer(color, comparisonColor)
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
	if(m_renderer.updateColor(color)) {
		refresh();
	}
}

void ColorPickItem::setComparisonColor(const QColor &comparisonColor)
{
	if(m_renderer.updateComparisonColor(comparisonColor)) {
		refresh();
	}
}

void ColorPickItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QColor baseColor = qApp->palette().color(QPalette::Base);
	baseColor.setAlpha(128);
	m_renderer.paint(painter, bounds(), baseColor);
}

}
