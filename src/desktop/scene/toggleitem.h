// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TOGGLEITEM_H
#define DESKTOP_TOGGLEITEM_H

#include <QGraphicsItem>
#include <QIcon>
#include <QPainterPath>

namespace drawingboard {

class ToggleItem final : public QGraphicsItem {
public:
	enum { Type = UserType + 16 };
	enum class Action {
		None,
		Left,
		Top,
		Right,
		Bottom,
	};

	ToggleItem(
		Action action, Qt::Alignment side, double fromTop, const QIcon &icon,
		QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	Action action() const { return m_action; }

	QRectF boundingRect() const override;

	void updatePosition(const QRectF &sceneBounds);

	bool checkHover(const QPointF &scenePos, bool &outWasHovering);
	void removeHover();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	static int totalSize() { return innerSize() + paddingSize() * 2; }
	static int paddingSize();
	static int innerSize();
	static int fontSize();

	static QPainterPath getLeftPath();
	static QPainterPath getRightPath();
	static QPainterPath makePath(bool right);

	Action m_action;
	bool m_right;
	double m_fromTop;
	QIcon m_icon;
	bool m_hover = false;
};

}

#endif
