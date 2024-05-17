// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_TRANSFORMITEM_H
#define DESKTOP_SCENE_TRANSFORMITEM_H
#include "desktop/scene/baseitem.h"
#include "libclient/utils/transformquad.h"

namespace drawingboard {

class TransformItem final : public BaseItem {
public:
	enum { Type = TransformType };

	TransformItem(
		const TransformQuad &quad, bool valid, qreal zoom,
		QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override { return m_boundingRect; }

	void setQuad(const TransformQuad &quad, bool valid);
	void setZoom(qreal zoom);
	void setToolState(int mode, int handle, bool dragging);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	static int calculateHandleSize(qreal zoom);
	QRectF calculateBoundingRect() const;

	void paintHandle(
		QPainter *painter, int handle, const QPointF &point, qreal angle);

	static const QPointF *getTransformHandlePoints(int handle, int &outCount);
	static const QPointF *getDistortHandlePoints(int handle, int &outCount);

	TransformQuad m_quad;
	int m_handleSize;
	QRectF m_boundingRect;
	int m_mode;
	int m_activeHandle;
	bool m_valid;
	bool m_dragging = false;
};

}

#endif
