// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_SELECTIONITEM_H
#define DESKTOP_SCENE_SELECTIONITEM_H
#include "desktop/scene/baseitem.h"

namespace canvas {
class Selection;
}

namespace drawingboard {

class SelectionItem final : public BaseObject {
public:
	enum { Type = SelectionType };

	SelectionItem(
		canvas::Selection *selection, QGraphicsItem *parent = nullptr);

	QRectF boundingRect() const override;
	int type() const override { return Type; }

	void marchingAnts(double dt);

private slots:
	void onShapeChanged();
	void onAdjustmentModeChanged();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *) override;

private:
	QPolygonF m_shape;
	canvas::Selection *m_selection;
	qreal m_marchingants;
};

}

#endif // SELECTIONITEM_H
