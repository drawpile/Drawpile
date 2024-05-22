// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/scene/transformitem.h"
#include "desktop/scene/arrows_data.h"
#include "libclient/tools/transform.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QTransform>

namespace drawingboard {

TransformItem::TransformItem(
	const TransformQuad &quad, bool valid, qreal zoom, QGraphicsItem *parent)
	: BaseItem(parent)
	, m_quad(quad)
	, m_handleSize(calculateHandleSize(zoom))
	, m_boundingRect(calculateBoundingRect())
	, m_mode(int(tools::TransformTool::Mode::Scale))
	, m_activeHandle(int(tools::TransformTool::Handle::None))
	, m_valid(valid)
{
}

void TransformItem::setQuad(const TransformQuad &quad, bool valid)
{
	if(quad != m_quad) {
		refreshGeometry();
		m_quad = quad;
		m_valid = valid;
		m_boundingRect = calculateBoundingRect();
		updatePreviewTransform();
	} else if(valid != m_valid) {
		m_valid = valid;
		updatePreviewTransform();
		refresh();
	}
}

void TransformItem::setPreviewImage(const QImage &image)
{
	bool imageChanged = image != m_previewImage;
	if(imageChanged) {
		m_previewImage = image;
		updatePreviewTransform();
		refresh();
	}
}

void TransformItem::setZoom(qreal zoom)
{
	int handleSize = calculateHandleSize(zoom);
	if(handleSize != m_handleSize) {
		refreshGeometry();
		m_handleSize = handleSize;
		m_boundingRect = calculateBoundingRect();
	}
}

void TransformItem::setToolState(int mode, int handle, bool dragging)
{
	if(mode != m_mode || handle != m_activeHandle || dragging != m_dragging) {
		m_mode = mode;
		m_activeHandle = handle;
		m_dragging = dragging;
		refresh();
	}
}

void TransformItem::paint(
	QPainter *painter, const QStyleOptionGraphicsItem *options, QWidget *widget)
{
	Q_UNUSED(options);
	Q_UNUSED(widget);
	if(!m_quad.isNull()) {
		painter->setRenderHint(QPainter::Antialiasing, false);
		painter->setRenderHint(QPainter::SmoothPixmapTransform, false);

		if(m_previewTransformValid) {
			painter->save();
			painter->setTransform(m_previewTransform, true);
			painter->drawImage(0.0, 0.0, m_previewImage);
			painter->restore();
		}

		QPen pen(m_valid ? Qt::gray : Qt::darkRed);
		pen.setCosmetic(true);
		qreal dpr = painter->device()->devicePixelRatioF();
		pen.setWidth(dpr * 3.0);
		painter->setPen(pen);
		painter->setOpacity(m_dragging ? 0.25 : 0.5);
		painter->drawPolygon(m_quad.polygon());

		pen.setColor(m_valid ? Qt::white : Qt::red);
		pen.setWidth(dpr);
		painter->setPen(pen);
		painter->setOpacity(m_dragging ? 0.5 : 1.0);
		painter->drawPolygon(m_quad.polygon());

		if(!m_dragging) {
			if(!m_valid) {
				pen.setColor(Qt::white);
				painter->setPen(pen);
			}
			painter->setBrush(Qt::black);
			painter->setRenderHint(QPainter::Antialiasing);
			paintHandle(
				painter, int(tools::TransformTool::Handle::TopLeft),
				m_quad.topLeft(), m_quad.topLeftAngle() - 315.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::Top), m_quad.top(),
				m_quad.topAngle() - 270.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::TopRight),
				m_quad.topRight(), m_quad.topRightAngle() - 225.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::Right),
				m_quad.right(), m_quad.rightAngle() - 180.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::BottomRight),
				m_quad.bottomRight(), m_quad.bottomRightAngle() - 135.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::Bottom),
				m_quad.bottom(), m_quad.bottomAngle() - 90.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::BottomLeft),
				m_quad.bottomLeft(), m_quad.bottomLeftAngle() - 45.0);
			paintHandle(
				painter, int(tools::TransformTool::Handle::Left), m_quad.left(),
				m_quad.leftAngle());
		}
	}
}

void TransformItem::updatePreviewTransform()
{
	if(m_valid && !m_previewImage.isNull()) {
		QRectF imageRect(m_previewImage.rect());
		QPolygonF imagePolygon({
			imageRect.topLeft(),
			imageRect.topRight(),
			imageRect.bottomRight(),
			imageRect.bottomLeft(),
		});
		m_previewTransformValid = QTransform::quadToQuad(
			imagePolygon, m_quad.polygon(), m_previewTransform);
	} else {
		m_previewTransformValid = false;
	}
}

int TransformItem::calculateHandleSize(qreal zoom)
{
	return zoom > 0.0 ? qRound(tools::TransformTool::HANDLE_SIZE / zoom) : 0;
}

QRectF TransformItem::calculateBoundingRect() const
{
	return m_quad.boundingRect().marginsAdded(
		QMarginsF(m_handleSize, m_handleSize, m_handleSize, m_handleSize));
}

void TransformItem::paintHandle(
	QPainter *painter, int handle, const QPointF &point, qreal angle)
{
	int count;
	const QPointF *points;
	switch(m_mode) {
	case int(tools::TransformTool::Mode::Scale):
		points = getTransformHandlePoints(handle, count);
		break;
	case int(tools::TransformTool::Mode::Distort):
		points = getDistortHandlePoints(handle, count);
		break;
	default:
		qWarning("TransformItem::paintHandle: unknown mode %d", m_mode);
		return;
	}

	if(count > 0 && points) {
		bool active = m_activeHandle == handle;
		qreal size = m_handleSize * (active ? 1.0 : 0.8);
		qreal halfSize = size * 0.5;

		QTransform matrix;
		matrix.translate(point.x(), point.y());
		matrix.rotate(360.0 - angle);
		matrix.translate(-halfSize, -halfSize);

		constexpr int MAX_POINTS = 12;
		QPointF polygon[MAX_POINTS];
		Q_ASSERT(count <= MAX_POINTS);
		for(int i = 0; i < count; ++i) {
			polygon[i] = matrix.map((points[i] / 10.0 * size));
		}

		painter->setOpacity(active ? 1.0 : 0.5);
		painter->drawPolygon(polygon, count);
	}
}

const QPointF *
TransformItem::getTransformHandlePoints(int handle, int &outCount)
{
	switch(handle) {
	case int(tools::TransformTool::Handle::TopLeft):
	case int(tools::TransformTool::Handle::BottomRight):
		outCount = sizeof(arrows::diag1) / sizeof(*arrows::diag1);
		return arrows::diag1;
	case int(tools::TransformTool::Handle::Top):
	case int(tools::TransformTool::Handle::Bottom):
		outCount = sizeof(arrows::vertical) / sizeof(*arrows::vertical);
		return arrows::vertical;
	case int(tools::TransformTool::Handle::TopRight):
	case int(tools::TransformTool::Handle::BottomLeft):
		outCount = sizeof(arrows::diag2) / sizeof(*arrows::diag2);
		return arrows::diag2;
	case int(tools::TransformTool::Handle::Right):
	case int(tools::TransformTool::Handle::Left):
		outCount = sizeof(arrows::horizontal) / sizeof(*arrows::horizontal);
		return arrows::horizontal;
	default:
		qWarning("TransformItem::getTransformArrow: unknown handle %d", handle);
		return nullptr;
	}
}

const QPointF *TransformItem::getDistortHandlePoints(int handle, int &outCount)
{
	switch(handle) {
	case int(tools::TransformTool::Handle::TopLeft):
	case int(tools::TransformTool::Handle::Top):
	case int(tools::TransformTool::Handle::TopRight):
	case int(tools::TransformTool::Handle::Right):
	case int(tools::TransformTool::Handle::BottomRight):
	case int(tools::TransformTool::Handle::Bottom):
	case int(tools::TransformTool::Handle::BottomLeft):
	case int(tools::TransformTool::Handle::Left):
		outCount = sizeof(arrows::distort) / sizeof(*arrows::distort);
		return arrows::distort;
	default:
		qWarning("TransformItem::getDistortArrow: unknown handle %d", handle);
		return nullptr;
	}
}

}
