// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_MASKPREVIEWITEM_H
#define DESKTOP_SCENE_MASKPREVIEWITEM_H
#include "desktop/scene/baseitem.h"
#include <QImage>

namespace drawingboard {

class MaskPreviewItem final : public BaseItem {
public:
	enum { Type = MaskPreviewType };

	MaskPreviewItem(const QImage &mask, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;

	int type() const override { return Type; }

	void setMask(const QImage &mask);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	QImage m_mask;
};

}

#endif
