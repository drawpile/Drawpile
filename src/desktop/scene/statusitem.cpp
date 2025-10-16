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
	, m_overflowMenu(new QMenu)
{
	setZValue(Z_STATUS);
	setStyle(style);
}

StatusItem::~StatusItem()
{
	QMenu *overflowMenu = m_overflowMenu.data();
	if(overflowMenu) {
		m_overflowMenu.clear();
		delete overflowMenu;
	}
}

QRectF StatusItem::boundingRect() const
{
	return m_bounds;
}

void StatusItem::setStyle(const QStyle *style)
{
	m_iconSize =
		qMax(8, style ? style->pixelMetric(QStyle::PM_ButtonIconSize) : 0);
	m_overflowIconSize =
		qMax(16, style ? style->pixelMetric(QStyle::PM_ToolBarIconSize) : 24);
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
		m_lastMenuIndex = -1;
		m_hoveredMenu = false;
		m_menuShown = false;
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
		m_hoveredIndex = -1;
		m_lastMenuIndex = -1;
		m_hoveredMenu = false;
		m_menuShown = false;
		return true;
	}
}

void StatusItem::checkHover(const QPointF &scenePos, HudAction &action)
{
	if(m_hoveredIndex != -1) {
		action.wasHovering = true;
	}

	int hoveredIndex = -1;
	bool hoveredMenu = false;
	int actionCount = m_actions.size();
	if(actionCount != 0) {
		QPointF pos = mapFromScene(scenePos);
		if(m_bounds.contains(pos)) {
			qreal y = pos.y() - m_bounds.top();
			if(y >= m_actionsBounds[0].top()) {
				for(int i = 0; i < actionCount; ++i) {
					if(y <= m_actionsBounds[i].bottom()) {
						hoveredIndex = i;
						action.action = m_actions[i];
						if(QMenu *menu = m_actions[i]->menu();
						   menu && pos.x() > m_bounds.right() -
												 qreal(m_overflowIconSize) -
												 MARGIN / 2.0) {
							copyActionsToOverflowMenu(menu);
							action.type = HudAction::Type::TriggerMenu;
							action.menu = m_overflowMenu;
							hoveredMenu = true;
							m_lastMenuIndex = i;
						} else {
							action.type = HudAction::Type::TriggerAction;
						}
						break;
					}
				}
			}
		}
	}

	if(hoveredIndex != m_hoveredIndex || hoveredMenu != m_hoveredMenu) {
		m_hoveredIndex = hoveredIndex;
		m_hoveredMenu = hoveredMenu;
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

void StatusItem::setMenuShown(bool menuShown)
{
	if(menuShown != m_menuShown) {
		m_menuShown = menuShown;
		refresh();
	}
}

void StatusItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QPalette pal = qApp->palette();
	int hoveredIndex;
	bool hoveredMenu;
	if(m_menuShown && m_lastMenuIndex != -1 &&
	   m_lastMenuIndex < int(m_actions.size()) &&
	   m_actions[m_lastMenuIndex]->menu()) {
		hoveredIndex = m_lastMenuIndex;
		hoveredMenu = true;
	} else {
		hoveredIndex = m_hoveredIndex;
		hoveredMenu = m_hoveredMenu;
	}

	painter->setRenderHint(QPainter::Antialiasing, false);
	painter->setPen(Qt::NoPen);
	painter->setBrush(pal.base());
	painter->drawRect(m_bounds);

	QPointF anchor = m_bounds.topLeft();
	qreal overflowIconSize = m_overflowIconSize;
	if(hoveredIndex != -1) {
		QRectF highlightBounds =
			m_actionsBounds[hoveredIndex].translated(anchor);
		if(m_actions[hoveredIndex]->menu()) {
			qreal split = m_bounds.right() - overflowIconSize - MARGIN / 2.0;
			if(hoveredMenu) {
				highlightBounds.setLeft(split);
				highlightBounds.setRight(m_bounds.right());
			} else {
				highlightBounds.setLeft(m_bounds.left());
				highlightBounds.setRight(split);
			}
		} else {
			highlightBounds.setLeft(m_bounds.left());
			highlightBounds.setRight(m_bounds.right());
		}
		painter->setBrush(pal.highlight());
		painter->drawRect(highlightBounds);
	}

	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setFont(m_font);
	painter->setPen(QPen(pal.text(), 1.0));
	painter->setBrush(Qt::NoBrush);

	bool haveText = !m_text.isEmpty();
	int actionCount = qMin(m_actions.size(), m_actionsBounds.size());
	for(int i = 0; i < actionCount; ++i) {
		QRectF actionBounds = m_actionsBounds[i];
		qreal y = actionBounds.top() + m_bounds.top();
		if(i != 0 || haveText) {
			painter->drawLine(m_bounds.left(), y, m_bounds.right(), y);
		}

		if(m_actions[i]->menu()) {
			qreal x = m_bounds.right() - MARGIN / 2.0 - overflowIconSize;
			painter->drawLine(x, y, x, actionBounds.bottom() + m_bounds.top());
		}
	}

	if(haveText) {
		painter->drawText(
			m_textBounds.translated(anchor).marginsRemoved(getTextMargins()),
			m_text);
	}

	QMarginsF actionMargins = getActionMargins();
	qreal iconSize = m_iconSize;
	QMarginsF actionTextMargins = actionMargins;
	actionTextMargins.setLeft(actionMargins.left() + iconSize * ICON_SPACE);

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

		if(action->menu()) {
			if(m_overflowIcon.isNull()) {
				m_overflowIcon = QIcon::fromTheme(
					QStringLiteral("drawpile_ellipsis_vertical"));
			}
			QRectF overflowIconRect = QRectF(
				m_bounds.right() - overflowIconSize - MARGIN / 4.0,
				actionBounds.top() +
					(actionBounds.height() - overflowIconSize) / 2.0,
				overflowIconSize, overflowIconSize);
			bool isOverflowHovered = hoveredIndex == i && hoveredMenu;
			if(!isOverflowHovered) {
				painter->setOpacity(0.8);
			}
			m_overflowIcon.paint(painter, overflowIconRect.toRect());
			if(!isOverflowHovered) {
				painter->setOpacity(1.0);
			}
		}
	}
}

void StatusItem::copyActionsToOverflowMenu(QMenu *menu)
{
	m_overflowMenu->clear();
	m_overflowMenu->addActions(menu->actions());
}

void StatusItem::updateBounds()
{
	refreshGeometry();
	QFontMetricsF fontMetrics(m_font);

	if(m_text.isEmpty()) {
		m_textBounds = QRectF(0.0, 0.0, 0.0, 0.0);
	} else {
		m_textBounds =
			fontMetrics.boundingRect(QRect(0, 0, 0xffff, 0xffff), 0, m_text)
				.marginsAdded(getTextMargins());
		m_textBounds.moveTo(0.0, 0.0);
	}
	m_bounds = m_textBounds;

	m_actionsBounds.clear();
	qreal iconSize = m_iconSize;
	qreal overflowIconSize = m_overflowIconSize;
	qreal nextBoundsY = m_textBounds.bottom();
	QMarginsF actionMargins = getActionMargins();
	for(int i = 0, count = m_actions.count(); i < count; ++i) {
		QAction *action = m_actions[i];

		QRectF actionBounds = fontMetrics.boundingRect(
			QRect(0, 0, 0xffff, 0xffff), 0, action->text());
		actionBounds.setRight(actionBounds.right() + iconSize * ICON_SPACE);
		if(action->menu()) {
			actionBounds.setRight(
				actionBounds.right() + overflowIconSize + MARGIN / 2.0);
		}
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
