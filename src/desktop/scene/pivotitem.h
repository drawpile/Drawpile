// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_PIVOTITEM_H
#define DESKTOP_SCENE_PIVOTITEM_H
#include "desktop/scene/baseitem.h"
#include <QPixmap>

namespace drawingboard {

class PivotItem final : public BaseItem {
public:
	enum { Type = PivotType };

	PivotItem(bool valid, qreal zoom, QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override { return m_boundingRect; }

	void setQuadValid(bool valid);
	void setZoom(qreal zoom);
	void setToolState(int handle, bool dragging, bool pivotShown);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	static int calculateHandleSize(qreal zoom);
	QRectF calculateBoundingRect() const;

	bool shouldShow() const
	{
		return m_valid && m_pivotShown && (m_active || !m_dragging);
	}

	static QPixmap paintHandlePixmap(qreal dpr);

	static QMarginsF handleMargins(qreal dpr, qreal n)
	{
		return QMarginsF(dpr * n, dpr * n, dpr * n, dpr * n);
	}

	QPixmap m_handlePixmap;
	qreal m_dpr = 1.0;
	int m_handleSize;
	QRectF m_boundingRect;
	bool m_valid;
	bool m_active = false;
	bool m_dragging = false;
	bool m_pivotShown = false;
};

}

#endif
