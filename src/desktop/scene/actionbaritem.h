// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_ACTIONBARITEM_H
#define DESKTOP_SCENE_ACTIONBARITEM_H
#include "desktop/scene/baseitem.h"
#include <QFont>
#include <QIcon>
#include <QMenu>
#include <QPointer>
#include <QVector>

class QAction;
class QMenu;
class QStyle;
struct HudAction;

namespace drawingboard {

class ActionBarItem final : public BaseItem {
public:
	enum { Type = ActionBarType };

	enum class Location {
		TopLeft,
		TopCenter,
		TopRight,
		BottomLeft,
		BottomCenter,
		BottomRight,
	};

	struct Button {
		QAction *action;
		QString text;
		QIcon icon;

		explicit Button(QAction *act);
		Button(QAction *act, const QString &txt);
		Button(QAction *act, const QIcon &icn);
		Button(QAction *act, const QString &txt, const QIcon &icn);
	};

	ActionBarItem(
		const QString &title, const QStyle *style, const QFont &font,
		QAction *overflowAction, QGraphicsItem *parent = nullptr);

	~ActionBarItem() override;

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	QMenu *overflowMenu() { return m_overflowMenu.data(); }
	void setMenuShown(bool menuShown);

	void setStyle(const QStyle *style);
	void setFont(const QFont &font);

	void setButtons(const QVector<Button> &buttons);
	void setOverflowMenuActions(const QVector<QAction *> &actions);

	void updateLocation(
		Location location, const QRectF &sceneBounds, double topOffset);

	void checkHover(const QPointF &scenePos, HudAction &action);
	void removeHover();

	static bool isLocationAtTop(Location location);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	enum class Hover {
		None,
		Button,
	};

	void updateBounds();

	QSizeF getTotalSize() const;
	qreal getButtonSize() const;

	static QPointF getPositionForLocation(
		Location location, const QRectF &sceneBounds, double topOffset);

	static QPointF
	getBoundsOffsetForLocation(Location location, const QRectF &bounds);

	Location m_location = Location::TopLeft;
	int m_hoveredIndex = -1;
	bool m_menuShown = false;
	int m_buttonInnerSize;
	int m_paddingSize;
	int m_fontHeight;
	QString m_title;
	QFont m_font;
	QAction *m_overflowAction;
	QPointer<QMenu> m_overflowMenu;
	QVector<Button> m_buttons;
	QPointF m_anchor;
	QRectF m_bounds;
};

}

#endif
