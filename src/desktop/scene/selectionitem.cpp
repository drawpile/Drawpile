// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/scene/selectionitem.h"
#include "desktop/scene/arrows_data.h"

#include <QPainter>
#include <QApplication>
#include <QStyleOptionGraphicsItem>

namespace drawingboard {

SelectionItem::SelectionItem(canvas::Selection *selection, QGraphicsItem *parent)
	: QGraphicsObject(parent), m_selection(selection), m_marchingants(0)
{
	Q_ASSERT(selection);
	connect(selection, &canvas::Selection::shapeChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::pasteImageChanged, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::closed, this, &SelectionItem::onShapeChanged);
	connect(selection, &canvas::Selection::adjustmentModeChanged, this, &SelectionItem::onAdjustmentModeChanged);
	m_shape = m_selection->shape();
}

void SelectionItem::onShapeChanged()
{
	prepareGeometryChange();
	m_shape = m_selection->shape();
}

void SelectionItem::onAdjustmentModeChanged()
{
	update();
}

QRectF SelectionItem::boundingRect() const
{
	const int h = m_selection ? m_selection->handleSize() : 0;
	return m_shape.boundingRect().adjusted(-h, -h, h, h);
}

static inline void drawPolygon(QPainter *painter, const QPolygonF &polygon, bool closed)
{
	if(closed)
		painter->drawPolygon(polygon);
	else
		painter->drawPolyline(polygon);
}

static const QPointF *getScaleArrow(canvas::Selection::Handle handle, unsigned int &outPlen)
{
	switch(handle) {
	case canvas::Selection::Handle::TopLeft:
	case canvas::Selection::Handle::BottomRight:
		outPlen = sizeof(arrows::diag1);
		return arrows::diag1;
	case canvas::Selection::Handle::TopRight:
	case canvas::Selection::Handle::BottomLeft:
		outPlen = sizeof(arrows::diag2);
		return arrows::diag2;
	case canvas::Selection::Handle::Top:
	case canvas::Selection::Handle::Bottom:
		outPlen = sizeof(arrows::vertical);
		return arrows::vertical;
	case canvas::Selection::Handle::Left:
	case canvas::Selection::Handle::Right:
		outPlen = sizeof(arrows::horizontal);
		return arrows::horizontal;
	default:
		return nullptr;
	}
}

static const QPointF *getRotationArrow(canvas::Selection::Handle handle, unsigned int &outPlen)
{
	switch(handle) {
	case canvas::Selection::Handle::TopLeft:
		outPlen = sizeof(arrows::rotate1);
		return arrows::rotate1;
	case canvas::Selection::Handle::TopRight:
		outPlen = sizeof(arrows::rotate2);
		return arrows::rotate2;
	case canvas::Selection::Handle::BottomRight:
		outPlen = sizeof(arrows::rotate3);
		return arrows::rotate3;
	case canvas::Selection::Handle::BottomLeft:
		outPlen = sizeof(arrows::rotate4);
		return arrows::rotate4;
	case canvas::Selection::Handle::Left:
	case canvas::Selection::Handle::Right:
		outPlen = sizeof(arrows::verticalSkew);
		return arrows::verticalSkew;
	case canvas::Selection::Handle::Top:
	case canvas::Selection::Handle::Bottom:
		outPlen = sizeof(arrows::horizontalSkew);
		return arrows::horizontalSkew;
	default:
		return nullptr;
	}
}

static const QPointF *getDistortArrow(canvas::Selection::Handle handle, unsigned int &outPlen)
{
	switch(handle) {
	case canvas::Selection::Handle::TopLeft:
		outPlen = sizeof(arrows::distortTopLeft);
		return arrows::distortTopLeft;
	case canvas::Selection::Handle::Top:
		outPlen = sizeof(arrows::distortTop);
		return arrows::distortTop;
	case canvas::Selection::Handle::TopRight:
		outPlen = sizeof(arrows::distortTopRight);
		return arrows::distortTopRight;
	case canvas::Selection::Handle::Right:
		outPlen = sizeof(arrows::distortRight);
		return arrows::distortRight;
	case canvas::Selection::Handle::BottomRight:
		outPlen = sizeof(arrows::distortBottomRight);
		return arrows::distortBottomRight;
	case canvas::Selection::Handle::Bottom:
		outPlen = sizeof(arrows::distortBottom);
		return arrows::distortBottom;
	case canvas::Selection::Handle::BottomLeft:
		outPlen = sizeof(arrows::distortBottomLeft);
		return arrows::distortBottomLeft;
	case canvas::Selection::Handle::Left:
		outPlen = sizeof(arrows::distortLeft);
		return arrows::distortLeft;
	default:
		return nullptr;
	}
}

typedef const QPointF *(*GetArrowFunction)(canvas::Selection::Handle handle, unsigned int &outPlen);

static GetArrowFunction mapModeToGetArrowFunction(canvas::Selection::AdjustmentMode mode)
{
	switch(mode) {
	case canvas::Selection::AdjustmentMode::Scale:
		return getScaleArrow;
	case canvas::Selection::AdjustmentMode::Rotate:
		return getRotationArrow;
	case canvas::Selection::AdjustmentMode::Distort:
		return getDistortArrow;
	default:
		qWarning("mapModeToGetArrowFunction: unknown mode");
		return nullptr;
	}
}

static void drawHandle(QPainter *painter, const QPointF &point, qreal size,
		canvas::Selection::Handle handle, GetArrowFunction getArrow)
{
	unsigned int plen;
	const QPointF *arrow = getArrow(handle, plen);

	if(arrow) {
		const QPointF offset = point - QPointF(size/2, size/2);

		QPointF polygon[12];
		Q_ASSERT(plen <= sizeof(polygon));
		for(unsigned int i=0;i<plen/sizeof(*polygon);++i)
			polygon[i] = offset + arrow[i] / 10 * size;

		painter->drawPolygon(polygon, plen/sizeof(*polygon));
	}
}

void SelectionItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *opt, QWidget *)
{
	painter->setClipRect(boundingRect().adjusted(-1, -1, 1, 1));

	QPen pen;
	pen.setWidth(qApp->devicePixelRatio());
	pen.setCosmetic(true);

	// Rectangular selections should always be *drawn* as closed
	const bool drawClosed = m_selection->isClosed() || m_shape.size() == 4;

	// Draw base color
	pen.setColor(Qt::black);
	painter->setPen(pen);
	drawPolygon(painter, m_shape, drawClosed);

	// Draw dashes
	pen.setColor(Qt::white);
	pen.setStyle(Qt::DashLine);
	pen.setDashOffset(m_marchingants);
	painter->setPen(pen);
	drawPolygon(painter, m_shape, drawClosed);

	// Draw resizing handles when initial drawing is finished
	GetArrowFunction getArrow;
	if(m_selection->isClosed() && (getArrow = mapModeToGetArrowFunction(m_selection->adjustmentMode()))) {
		pen.setStyle(Qt::SolidLine);
		painter->setPen(pen);
		painter->setBrush(Qt::black);

		const QRect rect = m_selection->boundingRect();
		const qreal s = m_selection->handleSize() / opt->levelOfDetailFromTransform(painter->transform());
		drawHandle(painter, rect.topLeft(), s, canvas::Selection::Handle::TopLeft, getArrow);
		drawHandle(painter, rect.topLeft() + QPointF(rect.width()/2, 0), s, canvas::Selection::Handle::Top, getArrow);
		drawHandle(painter, rect.topRight(), s, canvas::Selection::Handle::TopRight, getArrow);

		drawHandle(painter, rect.topLeft() + QPointF(0, rect.height()/2), s, canvas::Selection::Handle::Left, getArrow);
		drawHandle(painter, rect.topRight() + QPointF(0, rect.height()/2), s, canvas::Selection::Handle::Right, getArrow);

		drawHandle(painter, rect.bottomLeft(), s, canvas::Selection::Handle::BottomLeft, getArrow);
		drawHandle(painter, rect.bottomLeft() + QPointF(rect.width()/2, 0), s, canvas::Selection::Handle::Bottom, getArrow);
		drawHandle(painter, rect.bottomRight(), s, canvas::Selection::Handle::BottomRight, getArrow);
	}
}

void SelectionItem::marchingAnts(double dt)
{
	m_marchingants += dt / 0.2;
	update();
}

}
