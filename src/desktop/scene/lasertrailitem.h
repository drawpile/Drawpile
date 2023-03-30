/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2022 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef LASERTRAILITEM_H
#define LASERTRAILITEM_H

#include <QGraphicsItem>
#include <QPen>

namespace drawingboard {

class LaserTrailItem final : public QGraphicsItem
{
public:
	enum { Type = UserType + 13 };

	LaserTrailItem(uint8_t owner, int persistenceMs, const QColor &color, QGraphicsItem *parent=nullptr);

	QRectF boundingRect() const override;
	int type() const override { return Type; }

	bool animationStep(qreal dt);

	void addPoint(const QPointF &point);

	uint8_t owner() const { return m_owner; }

protected:
	void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

private:
	static constexpr float ANIM_TIME = 0.4f;

	float m_blink;
	bool m_fadeout;
	uint8_t m_owner;
	QPen m_pen;
	QVector<QPointF> m_points;
	QRectF m_bounds;
	qint64 m_lastModified;
	qint64 m_persistence;
};

}

#endif // LASERTRAILITEM_H
