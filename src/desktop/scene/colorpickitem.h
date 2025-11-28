// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_COLORPICKITEM_H
#define DESKTOP_COLORPICKITEM_H
#include "desktop/scene/baseitem.h"
#include "libclient/view/colorpickrenderer.h"
#include <QColor>

namespace drawingboard {

class ColorPickItem final : public BaseItem {
public:
	enum { Type = ColorPickType };

	ColorPickItem(
		const QColor &color, const QColor &comparisonColor,
		QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setColor(const QColor &color);
	void setComparisonColor(const QColor &comparisonColor);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	static QRect bounds()
	{
		int size = view::ColorPickRenderer::SIZE;
		int offset = size / -2;
		return QRect(offset, offset, size, size);
	}

	view::ColorPickRenderer m_renderer;
};

}

#endif
