// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_TRANSFORMQUAD_H
#define LIBCLIENT_UTILS_TRANSFORMQUAD_H
#include <QLineF>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QTransform>
#include <cmath>

class TransformQuad {
public:
	static constexpr int TOP_LEFT = 0;
	static constexpr int TOP_RIGHT = 1;
	static constexpr int BOTTOM_RIGHT = 2;
	static constexpr int BOTTOM_LEFT = 3;

	TransformQuad() {}

	TransformQuad(
		const QPointF &topLeft, const QPointF &topRight,
		const QPointF &bottomRight, const QPointF &bottomLeft,
		const QPointF &pivot = QPointF(0.5, 0.5))
		: m_polygon({topLeft, topRight, bottomRight, bottomLeft})
		, m_pivot(pivot)
	{
	}

	explicit TransformQuad(
		const QRectF &rect, const QPointF &pivot = QPointF(0.5, 0.5))
		: TransformQuad(
			  rect.topLeft(), rect.topRight(), rect.bottomRight(),
			  rect.bottomLeft(), pivot)
	{
	}

	explicit TransformQuad(
		const QRect &rect, const QPointF &pivot = QPointF(0.5, 0.5))
		: TransformQuad(QRectF(rect), pivot)
	{
	}

	explicit TransformQuad(
		const QPolygonF &polygon, const QPointF &pivot = QPointF(0.5, 0.5))
		: m_polygon(polygonToQuad(polygon))
		, m_pivot(pivot)
	{
	}

	const QPolygonF &polygon() const { return m_polygon; }

	TransformQuad round() const
	{
		QPolygonF polygon;
		for(const QPointF &point : m_polygon) {
			polygon.append(
				QPointF(std::round(point.x()), std::round(point.y())));
		}
		return TransformQuad(polygon, m_pivot);
	}

	QRectF boundingRect() const { return m_polygon.boundingRect(); }

	bool isNull() const { return m_polygon.isEmpty(); }

	void clear() { m_polygon.clear(); }

	bool map(const QTransform &matrix, TransformQuad &outResult) const
	{
		outResult.m_polygon = matrix.map(m_polygon);
		// Transforming with an "impossible" matrix results in polygons that
		// inexplicably have the wrong number of points.
		return outResult.m_polygon.size() == 4;
	}

	void applyOffset(int x, int y)
	{
		for(QPointF &point : m_polygon) {
			point.setX(point.x() + x);
			point.setY(point.y() + y);
		}
	}

	bool containsPoint(const QPointF &point) const
	{
		return m_polygon.containsPoint(point, Qt::OddEvenFill);
	}

	bool operator==(const TransformQuad &other) const
	{
		return m_polygon == other.m_polygon && m_pivot == other.m_pivot;
	}

	bool operator!=(const TransformQuad &other) const
	{
		return m_polygon != other.m_polygon || m_pivot != other.m_pivot;
	}

	QPointF topLeft() const
	{
		Q_ASSERT(!isNull());
		return m_polygon[0];
	}

	QPointF topRight() const
	{
		Q_ASSERT(!isNull());
		return m_polygon[1];
	}

	QPointF bottomRight() const
	{
		Q_ASSERT(!isNull());
		return m_polygon[2];
	}

	QPointF bottomLeft() const
	{
		Q_ASSERT(!isNull());
		return m_polygon[3];
	}

	QPointF top() const { return QLineF(topLeft(), topRight()).center(); }

	QPointF right() const { return QLineF(topRight(), bottomRight()).center(); }

	QPointF bottom() const
	{
		return QLineF(bottomLeft(), bottomRight()).center();
	}

	QPointF left() const { return QLineF(topLeft(), bottomLeft()).center(); }

	QPointF center() const
	{
		Q_ASSERT(!isNull());
		return m_polygon.boundingRect().center();
	}

	QPointF at(int i) const { return m_polygon.at(i); }

	qreal topAngle() const { return QLineF(top(), bottom()).angle(); }
	qreal rightAngle() const { return QLineF(right(), left()).angle(); }
	qreal bottomAngle() const { return QLineF(bottom(), top()).angle(); }
	qreal leftAngle() const { return QLineF(left(), right()).angle(); }

	qreal topLeftAngle() const { return (topAngle() + leftAngle()) / 2.0; }

	qreal topRightAngle() const { return (topAngle() + rightAngle()) / 2.0; }

	qreal bottomRightAngle() const
	{
		return (bottomAngle() + rightAngle()) / 2.0;
	}

	qreal bottomLeftAngle() const
	{
		return (bottomAngle() + leftAngle()) / 2.0;
	}

	qreal topEdgeAngle() const { return QLineF(topLeft(), topRight()).angle(); }

	qreal rightEdgeAngle() const
	{
		return QLineF(topRight(), bottomRight()).angle();
	}

	qreal bottomEdgeAngle() const
	{
		return QLineF(bottomRight(), bottomLeft()).angle();
	}

	qreal leftEdgeAngle() const
	{
		return QLineF(bottomLeft(), topLeft()).angle();
	}

	QPointF pivot() const { return m_pivot; }

	QPointF pivotPos() const
	{
		return QLineF(
				   QLineF(topLeft(), topRight()).pointAt(m_pivot.x()),
				   QLineF(bottomLeft(), bottomRight()).pointAt(m_pivot.x()))
			.pointAt(m_pivot.y());
	}

	bool isCenterPivot() const { return m_pivot == centerPivot(); }

	static constexpr QPointF centerPivot() { return QPointF(0.5, 0.5); }

private:
	static QPolygonF polygonToQuad(const QPolygonF &polygon)
	{
		switch(polygon.size()) {
		case 4:
			return polygon;
		case 0:
			return QPolygonF();
		default:
			qWarning(
				"TransformQuad: invalid polygon given, it has %d point(s)",
				int(polygon.size()));
			return QPolygonF();
		}
	}

	QPolygonF m_polygon;
	QPointF m_pivot = centerPivot();
};

#endif
