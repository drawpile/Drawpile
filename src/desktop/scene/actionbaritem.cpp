// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/actionbaritem.h"
#include "desktop/utils/widgetutils.h"
#include "libclient/view/hudaction.h"
#include <QAction>
#include <QApplication>
#include <QFont>
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
	: BaseObject(parent)
	, m_title(title)
	, m_overflowAction(overflowAction)
	, m_overflowMenu(new QMenu)
{
	setZValue(Z_ACTIONBAR);
	setStyle(style);
	setFont(font);
	setButtons({});
	setOverflowMenuActions({});
}

ActionBarItem::~ActionBarItem()
{
	QMenu *overflowMenu = m_overflowMenu.data();
	if(overflowMenu) {
		m_overflowMenu.clear();
		delete overflowMenu;
	}
}

QRectF ActionBarItem::boundingRect() const
{
	return m_bounds;
}

void ActionBarItem::setMenuShown(bool menuShown)
{
	if(menuShown != m_menuShown) {
		m_menuShown = menuShown;
		refresh();
	}
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
	for(const Button &button : m_buttons) {
		disconnect(
			button.action, &QAction::toggled, this, &ActionBarItem::refresh);
	}
	for(const Button &button : buttons) {
		connect(
			button.action, &QAction::toggled, this, &ActionBarItem::refresh);
	}
	m_buttons = buttons;
	m_buttons.append(Button(m_overflowAction));
	updateBounds();
}

void ActionBarItem::setOverflowMenuActions(const QVector<QAction *> &actions)
{
	m_overflowMenu->clear();
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
	if(m_hoveredIndex != -1) {
		action.wasHovering = true;
	}

	QPointF pos = mapFromScene(scenePos);
	int hoveredIndex = -1;
	if(m_bounds.contains(pos)) {
		bool overButtons;
		qreal pad = qreal(m_paddingSize);
		if(isLocationAtTop(m_location)) {
			overButtons = pos.y() <= m_bounds.y() + m_buttonInnerSize;
		} else {
			overButtons = pos.y() >= m_bounds.y() + m_fontHeight + pad * 2.0;
		}

		if(overButtons) {
			int lastButtonIndex = m_buttons.size() - 1;
			qreal buttonX = pos.x() - m_bounds.left();
			for(hoveredIndex = 0; hoveredIndex < lastButtonIndex;
				++hoveredIndex) {
				if(buttonX <= m_buttons[hoveredIndex].rect.right()) {
					break;
					;
				}
			}

			if(hoveredIndex < lastButtonIndex) {
				action.type = HudAction::Type::TriggerAction;
				action.action = m_buttons[hoveredIndex].action;
			} else {
				action.type = HudAction::Type::TriggerMenu;
				action.menu = m_overflowMenu;
			}
		}
	}

	if(hoveredIndex != m_hoveredIndex) {
		m_hoveredIndex = hoveredIndex;
		refresh();
	}
}

void ActionBarItem::removeHover()
{
	if(m_hoveredIndex != -1) {
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
	int hoveredIndex = m_menuShown ? m_buttons.size() - 1 : m_hoveredIndex;

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
	painter->setBrush(pal.base());
	painter->setPen(Qt::NoPen);
	if(hoveredIndex == -1) {
		painter->setOpacity(0.8);
	}

	if(hoveredIndex == -1 && m_title.isEmpty()) {
		painter->drawRect(buttonsRect);
	} else {
		painter->drawRect(m_bounds);
	}

	int buttonCount = m_buttons.size();
	painter->setBrush(pal.highlight());
	painter->setPen(Qt::NoPen);
	painter->setOpacity(hoveredIndex == -1 ? 0.5 : 0.3);
	for(int i = 0; i < buttonCount; ++i) {
		if(m_buttons[i].action->isChecked()) {
			painter->drawRect(
				m_buttons[i].rect.translated(buttonsRect.topLeft()));
		}
	}

	QString text;
	if(hoveredIndex != -1) {
		text = m_buttons[hoveredIndex].text;
		painter->setBrush(pal.highlight());
		painter->setOpacity(1.0);
		painter->drawRect(
			m_buttons[hoveredIndex].rect.translated(buttonsRect.topLeft()));
	} else {
		text = m_title;
	}

	painter->setRenderHint(QPainter::Antialiasing, true);
	painter->setPen(QPen(pal.text(), 1.0));
	painter->setBrush(Qt::NoBrush);

	if(!text.isEmpty()) {
		QTextOption textOption;
		textOption.setAlignment(Qt::AlignCenter);
		painter->setFont(m_font);
		painter->setOpacity(1.0);
		painter->drawText(textRect, text, textOption);
	}

	for(int i = 0; i < buttonCount; ++i) {
		QRectF iconRect = m_buttons[i].rect;
		QMarginsF iconMargins;
		if(i == 0 || i == buttonCount - 1 || buttonCount <= MAX_BUTTON_COUNT) {
			iconMargins = QMarginsF(pad, pad, pad, pad);
		} else {
			qreal vpad = 1.0 + ((iconRect.height() - iconRect.width()) / 2.0);
			iconMargins = QMarginsF(1.0, vpad, 1.0, vpad);
		}
		painter->setOpacity(i == hoveredIndex ? 1.0 : 0.5);
		m_buttons[i].icon.paint(
			painter, iconRect.translated(buttonsRect.topLeft())
						 .marginsRemoved(iconMargins)
						 .toRect());
	}

	painter->setOpacity(0.5);
	if(buttonCount <= MAX_BUTTON_COUNT) {
		for(int i = 1; i < buttonCount; ++i) {
			qreal x = buttonsRect.left() + m_buttons[i].rect.left();
			painter->drawLine(x, buttonsRect.top(), x, buttonsRect.bottom());
		}
	} else {
		qreal x1 = buttonsRect.left() + m_buttons[0].rect.right() - 1.0;
		qreal x2 =
			buttonsRect.left() + m_buttons[buttonCount - 1].rect.left() + 1.0;
		painter->drawLine(x1, buttonsRect.top(), x1, buttonsRect.bottom());
		painter->drawLine(x2, buttonsRect.top(), x2, buttonsRect.bottom());
	}
}

void ActionBarItem::updateBounds()
{
	refreshGeometry();
	m_bounds = QRectF(QPointF(0.0, 0.0), getTotalSize());
	m_bounds.moveTo(getBoundsOffsetForLocation(m_location, m_bounds));

	int buttonCount = m_buttons.size();
	qreal buttonSize = getButtonSize();
	if(buttonCount <= MAX_BUTTON_COUNT) {
		for(int i = 0; i < buttonCount; ++i) {
			m_buttons[i].rect =
				QRectF(qreal(i) * buttonSize, 0.0, buttonSize, buttonSize);
		}
	} else {
		// First and last buttons have normal size.
		int lastButton = buttonCount - 1;
		m_buttons[0].rect = QRectF(0.0, 0.0, buttonSize, buttonSize);
		m_buttons[lastButton].rect = QRectF(
			qreal(MAX_BUTTON_COUNT - 1) * buttonSize, 0.0, buttonSize,
			buttonSize);
		// Everything in the middle gets squashed.
		qreal middleButtonWidth =
			(buttonSize * qreal(MAX_BUTTON_COUNT - 2)) / qreal(buttonCount - 2);
		for(int i = 1; i < lastButton; ++i) {
			m_buttons[i].rect = QRectF(
				buttonSize + (qreal(i - 1) * middleButtonWidth), 0.0,
				middleButtonWidth, buttonSize);
		}
	}
}

QSizeF ActionBarItem::getTotalSize() const
{
	qreal pad = qreal(m_paddingSize);
	qreal buttonSize = getButtonSize();
	qreal width =
		qreal(qMin(int(m_buttons.size()), MAX_BUTTON_COUNT)) * buttonSize;
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
