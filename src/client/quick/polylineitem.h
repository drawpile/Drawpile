/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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

#ifndef POLYLINEITEM_H
#define POLYLINEITEM_H

#include <QQuickItem>

class PolyLineItem : public QQuickItem
{
	Q_PROPERTY(QVector<QPointF> points READ points WRITE setPoints NOTIFY pointsChanged)
	Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)
	Q_PROPERTY(int lineWidth READ lineWidth WRITE setLineWidth NOTIFY lineWidthChanged)
	Q_PROPERTY(Style lineStyle READ lineStyle WRITE setLineStyle NOTIFY lineStyleChanged)
	Q_OBJECT
public:
	enum Style {
		LaserScan,
		MarchingAnts
	};
	Q_ENUMS(Style)

	explicit PolyLineItem(QQuickItem *parent=nullptr);

	QVector<QPointF> points() const { return m_points; }
	void setPoints(const QVector<QPointF> &points);

	QColor color() const { return m_color; }
	void setColor(const QColor &color);

	int lineWidth() const { return m_lineWidth; }
	void setLineWidth(int w);

	Style lineStyle() const { return m_style; }
	void setLineStyle(Style style);

	QSGNode *updatePaintNode(QSGNode *, UpdatePaintNodeData *);

protected:
	void timerEvent(QTimerEvent *e);

signals:
	void pointsChanged();
	void colorChanged();
	int lineWidthChanged(int width);
	void lineStyleChanged();

private:
	QVector<QPointF> m_points;
	QColor m_color;
	int m_lineWidth;
	Style m_style;

	bool m_pointsChanged, m_colorChanged, m_lineWidthChanged, m_styleChanged;
};

#endif // POLYLINEITEM_H
