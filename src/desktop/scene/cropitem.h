// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_CROPITEM_H
#define DESKTOP_SCENE_CROPITEM_H
#include "desktop/scene/baseitem.h"

class QPainterPath;

namespace drawingboard {

class CropItem final : public BaseItem {
public:
	enum { Type = CropType };

	CropItem(
		QRect rect, int guides, qreal zoom, QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override { return m_boundingRect; }

	void setRect(QRect rect);
	void setGuides(int guides);
	void setZoom(qreal zoom);

	static int defaultGuides();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	int calculateHandleSize(qreal zoom) const;
	QRectF calculateBoundingRect() const;

	void addGuidesNths(QPainterPath &path, int n);

	QRect m_rect;
	int m_guides;
	int m_handleSize = 1;
	QRectF m_boundingRect;
	QPainterPath m_path;
};

}

#endif
