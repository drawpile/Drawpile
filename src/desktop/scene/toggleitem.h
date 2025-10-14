// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_TOGGLEITEM_H
#define DESKTOP_SCENE_TOGGLEITEM_H
#include "desktop/scene/baseitem.h"
#include "desktop/scene/hudaction.h"
#include <QIcon>
#include <QPainterPath>

namespace drawingboard {

class ToggleItem final : public BaseItem {
public:
	enum { Type = ToggleType };

	ToggleItem(
		HudAction::Type hudActionType, Qt::Alignment side, double fromTop,
		QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	HudAction::Type hudActionType() const { return m_hudActionType; }
	void setHudActionType(HudAction::Type hudActionType);

	QRectF boundingRect() const override;

	void updateSceneBounds(const QRectF &sceneBounds);

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

	static QIcon getIconForHudActionType(HudAction::Type hudActionType);

	HudAction::Type m_hudActionType = HudAction::Type::None;
	bool m_right;
	double m_fromTop;
	QIcon m_icon;
	bool m_hover = false;
};

}

#endif
