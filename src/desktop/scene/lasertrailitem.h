// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_LASERTRAILITEM_H
#define DESKTOP_SCENE_LASERTRAILITEM_H
#include "desktop/scene/baseitem.h"
#include <QPen>

namespace drawingboard {

class LaserTrailItem final : public BaseItem {
public:
	enum { Type = LaserTrailType };

	LaserTrailItem(
		int owner, int persistenceMs, const QColor &color,
		QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	int type() const override { return Type; }

	bool animationStep(qreal dt);

	void addPoint(const QPointF &point);

	int owner() const { return m_owner; }

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget) override;

private:
	static constexpr float ANIM_TIME = 0.4f;

	float m_blink = 0.0f;
	bool m_fadeout = false;
	int m_owner;
	QPen m_pen;
	QVector<QPointF> m_points;
	QRectF m_bounds;
	qint64 m_lastModified;
	qint64 m_persistence;
};

}

#endif // LASERTRAILITEM_H
