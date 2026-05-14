// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_TOGGLEITEM_H
#define DESKTOP_SCENE_TOGGLEITEM_H
#include "desktop/scene/baseitem.h"
#include "libclient/view/hudaction.h"
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
	int totalSize() const { return innerSize() + paddingSize() * 2; }
	int paddingSize() const;
	int innerSize() const;
	int fontSize() const;

	QPainterPath getLeftPath() const;
	QPainterPath getRightPath() const;
	QPainterPath makePath(bool right) const;

	static QIcon getIconForHudActionType(HudAction::Type hudActionType);

	HudAction::Type m_hudActionType = HudAction::Type::None;
	bool m_hover = false;
	bool m_right;
	double m_dpr = 1.0;
	double m_fromTop;
	QIcon m_icon;
};

}

#endif
