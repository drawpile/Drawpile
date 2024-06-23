// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_SCENE_SELECTIONITEM_H
#define DESKTOP_SCENE_SELECTIONITEM_H
#include "desktop/scene/baseitem.h"
#include <QImage>
#include <QPainterPath>

struct SelectionOutlinePath;

namespace drawdance {
class Selection;
}

namespace drawingboard {

class SelectionItem final : public BaseObject {
	Q_OBJECT
public:
	enum { Type = SelectionType };

	SelectionItem(bool ignored, bool showMask, QGraphicsItem *parent = nullptr);

	int type() const override { return Type; }

	QRectF boundingRect() const override;

	void setModel(const QRect &bounds, const QImage &mask);

	void setTransparentDelay(qreal transparentDelay);

	void setIgnored(bool ignored);
	void setShowMask(bool showMask);

	void animationStep(qreal dt);

signals:
	void outlineRegenerating();

protected:
	void paint(
		QPainter *painter, const QStyleOptionGraphicsItem *options,
		QWidget *widget) override;

private:
	bool canShowOutline() const;
	void setOutline(unsigned int executionId, const SelectionOutlinePath &path);
	void updateBoundingRectFromBounds();

	QRectF m_boundingRect;
	QRect m_bounds;
	QPainterPath m_path;
	QImage m_mask;
	qreal m_marchingAnts = 0.0;
	qreal m_maskOpacity = 0.0;
	qreal m_transparentDelay = 0.0;
	unsigned int m_executionId = 0;
	bool m_haveTemporaryMask = false;
	bool m_ignored;
	bool m_showMask;
};

}

#endif
