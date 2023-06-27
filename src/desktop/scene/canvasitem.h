// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DP_CANVASITEM_H
#define DP_CANVASITEM_H

#include <QGraphicsObject>
#include <QPointer>

namespace canvas {
	class PaintEngine;
}

namespace drawingboard {

/**
 * @brief A graphics item that draws a LayerStack
 */
class CanvasItem final : public QGraphicsObject
{
Q_OBJECT
public:
	CanvasItem(QGraphicsItem *parent=nullptr);
	void setPaintEngine(canvas::PaintEngine *pe);

	QRectF boundingRect() const override;

	void setViewportBounds(const QRectF viewportBounds);

	void setPixelGrid(bool pixelGrid);

private slots:
	void refreshImage(const QRect &area);
	void canvasResize();

protected:
	void paint(QPainter*, const QStyleOptionGraphicsItem*, QWidget*) override;

private:
	void updateVisibleArea();

	QPointer<canvas::PaintEngine> m_image;
	QRectF m_boundingRect;
	QRectF m_viewportBounds;
	QRect m_visibleArea;
	bool m_pixelGrid;
};

}

#endif
