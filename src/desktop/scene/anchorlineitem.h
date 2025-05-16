// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_ANCHORLINEITEM_H
#define DESKTOP_SCENE_ANCHORLINEITEM_H
#include "desktop/scene/baseitem.h"
#include <QPainterPath>
#include <QPointF>
#include <QVector>

namespace drawingboard {

class AnchorLineItem final : public BaseItem {
public:
	enum { Type = AnchorLineItemType };

	AnchorLineItem(
		const QVector<QPointF> &points, int activeIndex, qreal zoom,
		QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;

	int type() const override { return Type; }

	void setPoints(const QVector<QPointF> &points, int activeIndex);
	void setActiveIndex(int activeIndex);
	void setZoom(qreal zoom);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	void updatePath();
	qreal calculateHandleRadius() const;

	QVector<QPointF> m_points;
	QPainterPath m_path;
	qreal m_zoom;
	int m_activeIndex = -1;
};

}

#endif
