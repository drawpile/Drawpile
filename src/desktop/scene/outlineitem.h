// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_OUTLINEITEM_H
#define DESKTOP_OUTLINEITEM_H
#include <QCursor>
#include <QGraphicsItem>

namespace drawingboard {

class OutlineItem final : public QGraphicsItem {
public:
	enum { Type = UserType + 18 };

	OutlineItem(QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setOutline(qreal outlineSize, qreal outlineWidth);
	void setSquare(bool square);
	void setVisibleInMode(bool visibleInMode);
	void setOnCanvas(bool onCanvas);

	void setOutlineWidth(qreal outlineWidth)
	{
		setOutline(m_outlineSize, outlineWidth);
	}

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	void updateVisibility();

	QRectF m_bounds;
	QRectF m_outerBounds;
	qreal m_outlineSize = 0.0;
	qreal m_outlineWidth = 1.0;
	bool m_square = false;
	bool m_visibleInMode = true;
	bool m_onCanvas = false;
};

}

#endif
