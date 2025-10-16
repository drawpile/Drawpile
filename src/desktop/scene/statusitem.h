// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_STATUSITEM_H
#define DESKTOP_SCENE_STATUSITEM_H
#include "desktop/scene/baseitem.h"
#include <QFont>
#include <QMarginsF>
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

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setStyle(const QStyle *style);
	void setFont(const QFont &font);

	bool setStatus(const QString &text, const QVector<QAction *> &actions);
	bool clearStatus();

	void checkHover(const QPointF &scenePos, HudAction &action);
	void removeHover();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	static constexpr qreal MARGIN = 16.0;
	static constexpr qreal ICON_SPACE = 1.1;

	void updateBounds();

	QMarginsF getTextMargins() const;
	QMarginsF getActionMargins() const;

	QFont m_font;
	int m_iconSize;
	QRectF m_textBounds;
	QRectF m_bounds;
	QString m_text;
	QVector<QAction *> m_actions;
	QVector<QRectF> m_actionsBounds;
	int m_hoveredIndex = -1;
};

}

#endif
