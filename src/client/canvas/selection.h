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

#ifndef SELECTION_H
#define SELECTION_H

#include "core/blendmodes.h"

#include <QObject>
#include <QImage>
#include <QPolygonF>

namespace net {
	class Client;
}

namespace canvas {

/**
 * @brief This class represents a canvas area selection
 *
 * A selection can contain in image to be pasted.
 */
class Selection : public QObject
{
	Q_PROPERTY(QPolygonF shape READ shape WRITE setShape NOTIFY shapeChanged)
	Q_PROPERTY(QImage pasteImage READ pasteImage WRITE setPasteImage NOTIFY pasteImageChanged)
	Q_PROPERTY(int handleSize READ handleSize CONSTANT)
	Q_OBJECT
public:
	enum Handle {OUTSIDE, TRANSLATE, RS_TOPLEFT, RS_TOPRIGHT, RS_BOTTOMRIGHT, RS_BOTTOMLEFT, RS_TOP, RS_RIGHT, RS_BOTTOM, RS_LEFT};

	explicit Selection(QObject *parent = 0);

	QPolygonF shape() const { return m_shape; }
	void setShape(const QPolygonF &shape);
	Q_INVOKABLE void setShapeRect(const QRect &rect);

	void addPointToShape(const QPointF &point);
	void closeShape();

	bool isClosed() const { return m_closedPolygon; }

	Handle handleAt(const QPointF &point, float zoom) const;
	void adjustGeometry(Handle handle, const QPoint &delta, bool keepAspect);

	bool isAxisAlignedRectangle() const;

	void setMovedFromCanvas(bool m) { m_movedFromCanvas = m; }
	bool isMovedFromCanvas() const { return m_movedFromCanvas; }

	QRect boundingRect() const { return m_shape.boundingRect().toRect(); }

	QImage shapeMask(const QColor &color, QPoint *offset=nullptr) const;

	void setPasteImage(const QImage &image);
	QImage pasteImage() const { return m_pasteImage; }

	void pasteToCanvas(net::Client *client, int layer) const;
	void fillCanvas(const QColor &color, paintcore::BlendMode::Mode mode, net::Client *client, int layer) const;

	int handleSize() const { return 10; }

public slots:
	void resetShape();
	void translate(const QPoint &offset);
	void scale(qreal x, qreal y);
	void rotate(float angle);

signals:
	void shapeChanged(const QPolygonF &shape);
	void pasteImageChanged(const QImage &image);
	void closed();

private:
	void adjust(int dx1, int dy1, int dx2, int dy2);
	void saveShape();

	QPolygonF m_shape;
	QPolygonF m_originalShape;

	QImage m_pasteImage;

	bool m_closedPolygon;
	bool m_movedFromCanvas;
};

}

#endif // SELECTION_H
