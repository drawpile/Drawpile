// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/toggleitem.h"
#include <QApplication>
#include <QPainter>
#include <QPalette>

namespace drawingboard {

ToggleItem::ToggleItem(
	HudAction::Type hudActionType, Qt::Alignment side, double fromTop,
	QGraphicsItem *parent)
	: BaseItem(parent)
	, m_right{side == Qt::AlignRight}
	, m_fromTop{fromTop}
{
	setHudActionType(hudActionType);
	setFlag(ItemIgnoresTransformations);
	setZValue(Z_TOGGLE);
}

void ToggleItem::setHudActionType(HudAction::Type hudActionType)
{
	if(hudActionType != m_hudActionType) {
		m_hudActionType = hudActionType;
		m_icon = getIconForHudActionType(hudActionType);
		refresh();
	}
}

QRectF ToggleItem::boundingRect() const
{
	qreal s = qreal(totalSize());
	return QRectF{0.0, 0.0, s, s};
}

void ToggleItem::updateSceneBounds(const QRectF &sceneBounds)
{
	qreal s = qreal(totalSize());
	updatePosition(QPointF(
		m_right ? sceneBounds.right() - s : sceneBounds.left(),
		sceneBounds.top() + sceneBounds.height() * m_fromTop -
			(m_right ? 0 : s)));
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
	painter->setBrush(qApp->palette().base());
	painter->drawPath(m_right ? getRightPath() : getLeftPath());
	int padding = paddingSize();
	int inner = innerSize();
	m_icon.paint(
		painter,
		QRect{
			padding + padding / 4 * (m_right ? 1 : -1), padding, inner, inner});

	qreal dpr = painter->device()->devicePixelRatioF();
	if(qFuzzyCompare(dpr, m_dpr)) {
		m_dpr = dpr;
		refresh();
	}
}

int ToggleItem::paddingSize() const
{
	return fontSize();
}

int ToggleItem::innerSize() const
{
	return fontSize() * 4;
}

int ToggleItem::fontSize() const
{
	QFont font = QApplication::font();
	int size;
	if(font.pixelSize() == -1) {
		size = font.pointSize() - 1;
	} else if(m_dpr > 1.0) {
		size = qRound((qreal(font.pixelSize()) / m_dpr) - 1.0);
	} else {
		size = font.pixelSize() - 1;
	}
	return size < 1 ? 8 : size;
}

QPainterPath ToggleItem::getLeftPath() const
{
	static QPainterPath path;
	if(path.isEmpty()) {
		path = makePath(false);
	}
	return path;
}

QPainterPath ToggleItem::getRightPath() const
{
	static QPainterPath path;
	if(path.isEmpty()) {
		path = makePath(true);
	}
	return path;
}

QPainterPath ToggleItem::makePath(bool right) const
{
	constexpr qreal RADIUS = 10.0;
	QPainterPath t;
	t.setFillRule(Qt::WindingFill);
	qreal s = qreal(totalSize());
	t.addRoundedRect(0.0, 0.0, s, s, RADIUS, RADIUS);
	t.addRect(right ? s / 2.0 : 0.0, 0.0, s / 2.0, s);
	return t.simplified();
}

QIcon ToggleItem::getIconForHudActionType(HudAction::Type hudActionType)
{
	switch(hudActionType) {
	case HudAction::Type::ToggleBrush:
		return QIcon::fromTheme(QStringLiteral("draw-brush"));
	case HudAction::Type::ToggleTimeline:
		return QIcon::fromTheme(QStringLiteral("keyframe"));
	case HudAction::Type::ToggleLayer:
		return QIcon::fromTheme(QStringLiteral("layer-visible-on"));
	case HudAction::Type::ToggleChat:
		return QIcon::fromTheme(QStringLiteral("edit-comment"));
	default:
		qWarning(
			"ToggleItem::setHudActionType: no icon for %d", int(hudActionType));
		return QIcon();
	}
}

}
