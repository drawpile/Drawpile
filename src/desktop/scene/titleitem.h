// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_TITLEITEM_H
#define DESKTOP_TITLEITEM_H
#include "desktop/scene/baseitem.h"
#include <QColor>

namespace drawingboard {

class TitleItem final : public BaseItem {
public:
	enum { Type = TitleType };
	explicit TitleItem(
		const QString &text, const QColor &color,
		QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	bool setText(const QString &text);
	void setColor(const QColor &color);

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *option,
		QWidget *widget = nullptr) override;

private:
	void updateBounds();

	QRectF m_textBounds;
	QRectF m_bounds;
	QString m_text;
	QColor m_color;
};

}

#endif
