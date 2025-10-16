// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/statusitem.h"
#include "desktop/scene/hudaction.h"
#include <QAction>
#include <QApplication>
#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPalette>
#include <QStyle>

namespace drawingboard {

StatusItem::StatusItem(
	const QStyle *style, const QFont &font, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_font(font)
{
	setZValue(Z_STATUS);
	setStyle(style);
}

QRectF StatusItem::boundingRect() const
{
	return m_bounds;
}

void StatusItem::setStyle(const QStyle *style)
{
	m_iconSize =
		qMax(8, style ? style->pixelMetric(QStyle::PM_ButtonIconSize) : 0);
	updateBounds();
}

void StatusItem::setFont(const QFont &font)
{
	m_font = font;
	updateBounds();
}

bool StatusItem::setStatus(
	const QString &text, const QVector<QAction *> &actions)
{
	if(text != m_text || actions != m_actions) {
		m_text = text;
		m_actions = actions;
		m_hoveredIndex = -1;
		updateBounds();
		return true;
	} else {
		return false;
	}
}

bool StatusItem::clearStatus()
{
	if(m_text.isEmpty() && m_actions.isEmpty()) {
		return false;
	} else {
		m_text.clear();
		m_actions.clear();
		return true;
	}
}

void StatusItem::checkHover(const QPointF &scenePos, HudAction &action)
{
	if(m_hoveredIndex != -1) {
		action.wasHovering = true;
	}

	int hoveredIndex = -1;
	int actionCount = m_actions.size();
	if(actionCount != 0) {
		QPointF pos = mapFromScene(scenePos);
		if(m_bounds.contains(pos)) {
			qreal y = pos.y() - m_bounds.top();
			if(y >= m_actionsBounds[0].top()) {
				for(int i = 0; i < actionCount; ++i) {
					if(y <= m_actionsBounds[i].bottom()) {
						hoveredIndex = i;
						action.type = HudAction::Type::TriggerAction;
						action.action = m_actions[i];
						break;
					}
				}
			}
		}
	}

	if(hoveredIndex != m_hoveredIndex) {
		m_hoveredIndex = hoveredIndex;
		refresh();
	}
}

void StatusItem::removeHover()
{
	if(m_hoveredIndex != -1) {
		m_hoveredIndex = -1;
		refresh();
	}
}

void StatusItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QPalette pal = qApp->palette();

	painter->setRenderHint(QPainter::Antialiasing, false);
	painter->setPen(Qt::NoPen);
	painter->setBrush(pal.base());
	painter->drawRect(m_bounds);

	QPointF anchor = m_bounds.topLeft();
	if(m_hoveredIndex != -1) {
		QRectF highlightBounds =
			m_actionsBounds[m_hoveredIndex].translated(anchor);
		highlightBounds.setLeft(m_bounds.left());
		highlightBounds.setRight(m_bounds.right());
		painter->setBrush(pal.highlight());
		painter->drawRect(highlightBounds);
	}

	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setFont(m_font);
	painter->setPen(QPen(pal.text(), 1.0));
	painter->setBrush(Qt::NoBrush);

	for(const QRectF &actionBounds : m_actionsBounds) {
		qreal y = actionBounds.top() + m_bounds.top();
		painter->drawLine(m_bounds.left(), y, m_bounds.right(), y);
	}

	painter->drawText(
		m_textBounds.translated(anchor).marginsRemoved(getTextMargins()),
		m_text);

	QMarginsF actionMargins = getActionMargins();
	qreal iconSize = m_iconSize;
	QMarginsF actionTextMargins = actionMargins;
	actionTextMargins.setLeft(actionMargins.left() + iconSize * ICON_SPACE);

	int actionCount = qMin(m_actions.size(), m_actionsBounds.size());
	for(int i = 0; i < actionCount; ++i) {
		const QAction *action = m_actions[i];
		QRectF actionBounds = m_actionsBounds[i].translated(anchor);
		QIcon actionIcon = action->icon();
		if(!actionIcon.isNull()) {
			QRectF iconRect = QRectF(
				actionBounds.left() + actionMargins.left(),
				actionBounds.top() + (actionBounds.height() - iconSize) / 2.0,
				iconSize, iconSize);
			actionIcon.paint(painter, iconRect.toRect());
		}
		painter->drawText(
			actionBounds.marginsRemoved(actionTextMargins), action->text());
	}
}

void StatusItem::updateBounds()
{
	refreshGeometry();
	QFontMetricsF fontMetrics(m_font);

	m_textBounds =
		fontMetrics.boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_text)
			.marginsAdded(getTextMargins());
	m_textBounds.moveTo(0.0, 0.0);
	m_bounds = m_textBounds;

	m_actionsBounds.clear();
	qreal iconSize = m_iconSize;
	qreal nextBoundsY = m_textBounds.bottom();
	QMarginsF actionMargins = getActionMargins();
	for(int i = 0, count = m_actions.count(); i < count; ++i) {
		QAction *action = m_actions[i];

		QRectF actionBounds = fontMetrics.boundingRect(
			QRect(0, 0, 0xffff, 0xffff), 0, action->text());
		actionBounds.setRight(actionBounds.right() + iconSize * ICON_SPACE);
		if(actionBounds.height() < iconSize) {
			actionBounds.setHeight(iconSize);
		}

		actionBounds = actionBounds.marginsAdded(actionMargins);
		actionBounds.moveTo(0.0, nextBoundsY);
		nextBoundsY = actionBounds.bottom();
		m_actionsBounds.append(actionBounds);
		m_bounds |= actionBounds;
	}

	// Currently always anchored at the top-right.
	m_bounds.moveTopRight(QPointF(0.0, 0.0));
}

QMarginsF StatusItem::getTextMargins() const
{
	if(m_actions.isEmpty()) {
		return QMarginsF(MARGIN, MARGIN, MARGIN, MARGIN);
	} else {
		return QMarginsF(MARGIN, MARGIN / 2.0, MARGIN, MARGIN / 2.0);
	}
}

QMarginsF StatusItem::getActionMargins() const
{
	return QMarginsF(MARGIN * 0.75, MARGIN / 2.0, MARGIN, MARGIN / 2.0);
}

}
