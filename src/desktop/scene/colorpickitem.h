// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_COLORPICKITEM_H
#define DESKTOP_COLORPICKITEM_H
#include "desktop/scene/baseitem.h"
#include <QColor>
#include <QPixmap>

namespace drawingboard {

// Emulated bitmap cursor for platforms like Emscripten that don't support them.
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

	static bool shouldShow(int source, int visibility);
	static int defaultVisibility();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	static constexpr int SIZE = 200;

	static QColor sanitizeColor(const QColor &color);

	static QRect bounds()
	{
		int offset = SIZE / -2;
		return QRect(offset, offset, SIZE, SIZE);
	}

	QColor m_color;
	QColor m_comparisonColor;
	QPixmap m_cache;
};

}

#endif
