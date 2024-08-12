// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_PATHPREVIEWITEM_H
#define DESKTOP_SCENE_PATHPREVIEWITEM_H
#include "desktop/scene/baseitem.h"
#include <QPainterPath>

namespace drawingboard {

class PathPreviewItem final : public BaseItem {
public:
	enum { Type = PathPreviewType };

	PathPreviewItem(
		const QPainterPath &path, qreal zoom, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;

	int type() const override { return Type; }

	void setPath(const QPainterPath &path);
	void setZoom(qreal zoom);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	QPainterPath m_path;
	qreal m_zoom;
};

}

#endif
