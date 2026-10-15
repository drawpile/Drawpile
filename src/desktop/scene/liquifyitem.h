// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_LIQUIFYITEM_H
#define DESKTOP_SCENE_LIQUIFYITEM_H
#include "desktop/scene/baseitem.h"
#include <QImage>

namespace drawingboard {

class LiquifyItem final : public BaseItem {
public:
	enum { Type = TransformType };

	LiquifyItem(QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override { return m_boundingRect; }

	void setImage(const QImage &image, QPoint offset);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	QRectF m_boundingRect;
	QImage m_image;
	QPoint m_offset;
};

}

#endif
