// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_STATUSITEM_H
#define DESKTOP_SCENE_STATUSITEM_H
#include "desktop/scene/baseitem.h"
#include <QFont>
#include <QIcon>
#include <QMarginsF>
#include <QMenu>
#include <QPointer>
#include <QVector>

class QAction;
class QStyle;
struct HudAction;

namespace drawingboard {

class StatusItem final : public BaseItem {
public:
	enum { Type = StatusType };

	StatusItem(
		const QStyle *style, const QFont &font,
		QGraphicsItem *parent = nullptr);

	~StatusItem() override;

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setStyle(const QStyle *style);
	void setFont(const QFont &font);

	bool setStatus(const QString &text, const QVector<QAction *> &actions);
	bool clearStatus();

	void checkHover(const QPointF &scenePos, HudAction &action);
	void removeHover();

	QMenu *overflowMenu() const { return m_overflowMenu.data(); }

	void setMenuShown(bool menuShown);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	static constexpr qreal MARGIN = 16.0;
	static constexpr qreal ICON_SPACE = 1.2;

	void copyActionsToOverflowMenu(QMenu *menu);

	void updateBounds();

	QMarginsF getTextMargins() const;
	QMarginsF getActionMargins() const;

	QFont m_font;
	QIcon m_overflowIcon;
	int m_iconSize;
	int m_overflowIconSize;
	QRectF m_textBounds;
	QRectF m_bounds;
	QString m_text;
	QVector<QAction *> m_actions;
	QVector<QRectF> m_actionsBounds;
	QPointer<QMenu> m_overflowMenu;
	int m_hoveredIndex = -1;
	int m_lastMenuIndex = -1;
	bool m_hoveredMenu = false;
	bool m_menuShown = false;
};

}

#endif
