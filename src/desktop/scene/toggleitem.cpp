// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/toggleitem.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

ToggleItem::ToggleItem(
	Action action, Qt::Alignment side, double fromTop, const QIcon &icon,
	QGraphicsItem *parent)
	: BaseItem{parent}
	, m_action{action}
	, m_right{side == Qt::AlignRight}
	, m_fromTop{fromTop}
	, m_icon{icon}
{
	setFlag(ItemIgnoresTransformations);
	setZValue(99999);
}

QRectF ToggleItem::boundingRect() const
{
	qreal s = qreal(totalSize());
	return QRectF{0.0, 0.0, s, s};
}

void ToggleItem::updatePosition(const QRectF &sceneBounds)
{
	qreal s = qreal(totalSize());
	setX(m_right ? sceneBounds.right() - s : sceneBounds.left());
	setY(sceneBounds.top() + sceneBounds.height() * m_fromTop - s / 2.0);
}

bool ToggleItem::checkHover(const QPointF &scenePos, bool &outWasHovering)
{
	if(m_hover) {
		outWasHovering = true;
	}
	qreal s = qreal(totalSize());
	bool hover = QRectF{x(), y(), s, s}.contains(scenePos);
	if(hover != m_hover) {
		m_hover = hover;
		refresh();
	}
	return hover;
}

void ToggleItem::removeHover()
{
	if(m_hover) {
		m_hover = false;
		refresh();
	}
}

void ToggleItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	painter->setRenderHint(QPainter::Antialiasing);
	painter->setOpacity(m_hover ? 1.0 : 0.5);
	painter->setPen(Qt::NoPen);
	painter->setBrush(qApp->palette().window());
	painter->drawPath(m_right ? getRightPath() : getLeftPath());
	int padding = paddingSize();
	int inner = innerSize();
	m_icon.paint(
		painter,
		QRect{
			padding + padding / 4 * (m_right ? 1 : -1), padding, inner, inner});
}

int ToggleItem::paddingSize()
{
	return fontSize();
}

int ToggleItem::innerSize()
{
	return fontSize() * 4;
}

int ToggleItem::fontSize()
{
	int size = QApplication::font().pointSize() - 1;
	return size < 1 ? 8 : size;
}

QPainterPath ToggleItem::getLeftPath()
{
	static QPainterPath path;
	if(path.isEmpty()) {
		path = makePath(false);
	}
	return path;
}

QPainterPath ToggleItem::getRightPath()
{
	static QPainterPath path;
	if(path.isEmpty()) {
		path = makePath(true);
	}
	return path;
}

QPainterPath ToggleItem::makePath(bool right)
{
	constexpr qreal RADIUS = 10.0;
	QPainterPath t;
	t.setFillRule(Qt::WindingFill);
	qreal s = qreal(totalSize());
	t.addRoundedRect(0.0, 0.0, s, s, RADIUS, RADIUS);
	t.addRect(right ? s / 2.0 : 0.0, 0.0, s / 2.0, s);
	return t.simplified();
}

}
