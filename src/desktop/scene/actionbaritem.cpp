// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/actionbaritem.h"
#include "desktop/utils/widgetutils.h"
#include <QAction>
#include <QApplication>
#include <QFont>
#include <QMenu>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QTextOption>


namespace drawingboard {

ActionBarItem::Button::Button(QAction *act)
	: Button(act, utils::scrubAccelerators(act->text()), act->icon())
{
}

ActionBarItem::Button::Button(QAction *act, const QString &txt)
	: Button(act, txt, act->icon())
{
}

ActionBarItem::Button::Button(QAction *act, const QIcon &icn)
	: Button(act, utils::scrubAccelerators(act->text()), icn)
{
}

ActionBarItem::Button::Button(
	QAction *act, const QString &txt, const QIcon &icn)
	: action(act)
	, text(txt)
	, icon(icn)
{
}

ActionBarItem::ActionBarItem(
	const QString &title, const QStyle *style, const QFont &font,
	QAction *overflowAction, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_title(title)
	, m_overflowAction(overflowAction)
{
	setZValue(Z_ACTIONBAR);
	setStyle(style);
	setFont(font);
	setButtons({});
	setOverflowMenuActions({});
}

QRectF ActionBarItem::boundingRect() const
{
	return m_bounds;
}

void ActionBarItem::setStyle(const QStyle *style)
{
	m_buttonInnerSize =
		qMax(16, style ? style->pixelMetric(QStyle::PM_ToolBarIconSize) : 24);
	m_paddingSize = qMax(
		2, style ? style->pixelMetric(QStyle::PM_ToolBarItemMargin) +
					   style->pixelMetric(QStyle::PM_ToolBarItemSpacing)
				 : 4);
}

void ActionBarItem::setFont(const QFont &font)
{
	m_font = font;
	m_fontHeight = QFontMetrics(m_font).height();
	updateBounds();
}

void ActionBarItem::setButtons(const QVector<Button> &buttons)
{
	m_buttons = buttons;
	m_buttons.append(Button(m_overflowAction));
	updateBounds();
	refresh();
}

void ActionBarItem::setOverflowMenuActions(const QVector<QAction *> &actions)
{
	if(m_overflowMenu) {
		m_overflowMenu->deleteLater();
	}

	m_overflowMenu = new QMenu;
	for(QAction *action : actions) {
		if(action) {
			m_overflowMenu->addAction(action);
		} else {
			m_overflowMenu->addSeparator();
		}
	}
}

void ActionBarItem::updateLocation(
	Location location, const QRectF &sceneBounds, double topOffset)
{
	if(location != m_location) {
		m_location = location;
		updateBounds();
	}
	updatePosition(getPositionForLocation(location, sceneBounds, topOffset));
}

void ActionBarItem::checkHover(const QPointF &scenePos, HudAction &action)
{
	if(m_hover != Hover::None) {
		action.wasHovering = true;
	}

	QPointF pos = mapFromScene(scenePos);
	Hover hover = Hover::None;
	int hoveredIndex = -1;
	if(m_bounds.contains(pos)) {
		bool overButtons;
		qreal pad = qreal(m_paddingSize);
		qreal buttonSize = getButtonSize();
		if(isLocationAtTop(m_location)) {
			overButtons = pos.y() <= m_bounds.y() + m_buttonInnerSize;
		} else {
			overButtons = pos.y() >= m_bounds.y() + m_fontHeight + pad * 2.0;
		}

		if(overButtons) {
			hover = Hover::Button;
			int lastButtonIndex = m_buttons.size() - 1;
			hoveredIndex = qBound(
				0, int((pos.x() - m_bounds.x()) / buttonSize), lastButtonIndex);
			if(hoveredIndex < lastButtonIndex) {
				action.type = HudAction::Type::TriggerAction;
				action.action = m_buttons[hoveredIndex].action;
			} else {
				action.type = HudAction::Type::TriggerMenu;
				action.menu = m_overflowMenu;
			}
		}
	}

	if(hover != m_hover || hoveredIndex != m_hoveredIndex) {
		m_hover = hover;
		m_hoveredIndex = hoveredIndex;
		refresh();
	}
}

void ActionBarItem::removeHover()
{
	if(m_hover != Hover::None) {
		m_hover = Hover::None;
		m_hoveredIndex = -1;
		refresh();
	}
}

bool ActionBarItem::isLocationAtTop(Location location)
{
	switch(location) {
	case Location::TopLeft:
	case Location::TopCenter:
	case Location::TopRight:
		return true;
	default:
		return false;
	}
}

void ActionBarItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *, QWidget *)
{
	QPalette pal = qApp->palette();
	qreal pad = qreal(m_paddingSize);
	qreal buttonSize = getButtonSize();
	bool top = isLocationAtTop(m_location);

	QRectF textRect(
		m_bounds.left() + pad, m_bounds.top() + pad,
		m_bounds.width() - pad * 2.0, m_fontHeight);
	if(top) {
		textRect.translate(0.0, buttonSize);
	}

	QRectF buttonsRect(
		m_bounds.left(), m_bounds.top(), m_bounds.width(), buttonSize);
	if(!top) {
		buttonsRect.translate(0.0, m_fontHeight + pad * 2.0);
	}

	painter->setRenderHint(QPainter::Antialiasing, false);
	painter->setBrush(pal.window());
	painter->setPen(Qt::NoPen);
	if(m_hover == Hover::None) {
		painter->setOpacity(0.8);
	}

	if(m_hover == Hover::None && m_title.isEmpty()) {
		painter->drawRect(buttonsRect);
	} else {
		painter->drawRect(m_bounds);
	}

	QString text;
	if(m_hover == Hover::Button) {
		text = m_buttons[m_hoveredIndex].text;
		painter->setBrush(pal.alternateBase());
		painter->drawRect(QRectF(
			m_bounds.x() + buttonSize * qreal(m_hoveredIndex),
			buttonsRect.top(), buttonSize, buttonSize));
	} else {
		text = m_title;
	}

	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(QPen(pal.windowText(), 1.0));
	painter->setBrush(Qt::NoBrush);

	if(!text.isEmpty()) {
		QTextOption textOption;
		textOption.setAlignment(Qt::AlignCenter);
		painter->setFont(m_font);
		painter->drawText(textRect, text, textOption);
	}

	int buttonCount = m_buttons.size();
	qreal buttonInnerSize = qreal(m_buttonInnerSize);
	for(int i = 0; i < buttonCount; ++i) {
		QRectF iconRect(
			m_bounds.x() + qreal(i) * buttonSize + pad, buttonsRect.top() + pad,
			buttonInnerSize, buttonInnerSize);
		painter->setOpacity(i == m_hoveredIndex ? 1.0 : 0.5);
		m_buttons[i].icon.paint(painter, iconRect.toRect());
	}

	painter->setOpacity(0.5);
	for(int i = 1; i < buttonCount; ++i) {
		qreal x = m_bounds.x() + qreal(i) * buttonSize;
		painter->drawLine(x, buttonsRect.top(), x, buttonsRect.bottom());
	}
}

void ActionBarItem::updateBounds()
{
	m_bounds = QRectF(QPointF(0.0, 0.0), getTotalSize());
	m_bounds.moveTo(getBoundsOffsetForLocation(m_location, m_bounds));
	refresh();
}

QSizeF ActionBarItem::getTotalSize() const
{
	qreal pad = qreal(m_paddingSize);
	qreal buttonSize = getButtonSize();
	qreal width = qreal(m_buttons.size()) * buttonSize;
	qreal height = qreal(m_fontHeight) + pad * 2.0 + buttonSize;
	return QSizeF(width, height);
}

qreal ActionBarItem::getButtonSize() const
{
	return qreal(m_buttonInnerSize) + qreal(m_paddingSize) * 2.0;
}

QPointF ActionBarItem::getPositionForLocation(
	Location location, const QRectF &sceneBounds, double topOffset)
{
	switch(location) {
	case Location::TopLeft:
		return QPointF(sceneBounds.left(), sceneBounds.top() + topOffset);
	case Location::TopCenter:
		return QPointF(sceneBounds.center().x(), sceneBounds.top() + topOffset);
	case Location::TopRight:
		return QPointF(sceneBounds.right(), sceneBounds.top() + topOffset);
	case Location::BottomLeft:
		return sceneBounds.bottomLeft();
	case Location::BottomCenter:
		return QPointF(sceneBounds.center().x(), sceneBounds.bottom());
	case Location::BottomRight:
		return sceneBounds.bottomRight();
	}
	qWarning(
		"ActionBarItem::getPositionForLocation: unknown location %d",
		int(location));
	return QPointF();
}

QPointF ActionBarItem::getBoundsOffsetForLocation(
	Location location, const QRectF &bounds)
{
	switch(location) {
	case Location::TopLeft:
		return -bounds.topLeft();
	case Location::TopCenter:
		return QPointF(-bounds.center().x(), -bounds.top());
	case Location::TopRight:
		return -bounds.topRight();
	case Location::BottomLeft:
		return -bounds.bottomLeft();
	case Location::BottomCenter:
		return QPointF(-bounds.center().x(), -bounds.bottom());
	case Location::BottomRight:
		return -bounds.bottomRight();
	}
	qWarning(
		"ActionBarItem::getBoundsOffsetForLocation: unknown location %d",
		int(location));
	return QPointF();
}

}
