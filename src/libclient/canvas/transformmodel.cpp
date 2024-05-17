// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpmsg/blend_mode.h>
}
#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/transformmodel.h"
#include "libclient/drawdance/image.h"

namespace canvas {

TransformModel::TransformModel(QObject *parent)
	: QObject(parent)
{
}

void TransformModel::beginFromCanvas(
	const QRect &srcBounds, const QImage &mask, const QImage &image,
	int sourceLayerId)
{
	clear();
	m_active = true;
	m_justApplied = false;
	m_sourceLayerId = sourceLayerId;
	m_srcBounds = srcBounds;
	m_dstQuad = TransformQuad(srcBounds);
	m_dstQuadValid = isQuadValid(m_dstQuad);
	m_mask = mask;
	m_image = image;
	emit transformChanged();
	emit transformCut(m_sourceLayerId, srcBounds, m_mask);
}

void TransformModel::beginFloating(const QRect &srcBounds, const QImage &image)
{
	clear();
	m_active = true;
	m_pasted = true;
	m_justApplied = false;
	m_sourceLayerId = 0;
	m_srcBounds = srcBounds;
	m_dstQuad = TransformQuad(srcBounds);
	m_dstQuadValid = isQuadValid(m_dstQuad);
	m_image = image;
	emit transformChanged();
}

void TransformModel::setDstQuad(const TransformQuad &dstQuad)
{
	if(m_active) {
		TransformQuad result = dstQuad.round();
		if(result != m_dstQuad && !result.boundingRect().isEmpty()) {
			m_dstQuad = result;
			m_dstQuadValid = isQuadValid(result);
			m_justApplied = false;
			emit transformChanged();
		}
	} else {
		qWarning("TransformModel::setMatrix: transform not active");
	}
}

void TransformModel::applyOffset(int x, int y)
{
	if(m_active) {
		m_srcBounds.translate(x, y);
		m_dstQuad.applyOffset(x, y);
		emit transformChanged();
	}
}

QVector<net::Message> TransformModel::applyActiveTransform(
	uint8_t contextId, int layerId, int interpolation, bool compatibilityMode,
	bool stamp)
{
	if(m_active && m_dstQuadValid) {
		if(m_pasted) {
			return applyFloating(
				contextId, layerId, interpolation, compatibilityMode, stamp);
		} else {
			if(stamp) {
				m_stamped = true;
			}
			return applyFromCanvas(
				contextId, layerId, interpolation, compatibilityMode);
		}
	} else {
		return {};
	}
}

void TransformModel::endActiveTransform(bool applied)
{
	if(m_active) {
		if(!m_pasted) {
			emit transformCutCleared();
		}
		clear();
		m_justApplied = applied;
		emit transformChanged();
	}
}

int TransformModel::getEffectiveInterpolation(int interpolation) const
{
	if(interpolation != DP_MSG_TRANSFORM_REGION_MODE_NEAREST &&
	   m_dstQuadValid) {
		QTransform t;
		if(QTransform::quadToQuad(
			   QPolygonF(QRectF(m_srcBounds)), m_dstQuad.polygon(), t)) {
			t.setMatrix(
				t.m11(), t.m12(), t.m13(), t.m21(), t.m22(), t.m23(), 0.0, 0.0,
				t.m33());
			if(isRightAngleRotationOrReflection(t)) {
				return DP_MSG_TRANSFORM_REGION_MODE_NEAREST;
			}
		}
	}
	return interpolation;
}

QVector<net::Message> TransformModel::applyFromCanvas(
	uint8_t contextId, int layerId, int interpolation, bool compatibilityMode)
{
	Q_ASSERT(m_active);
	Q_ASSERT(!m_pasted || m_stamped);
	constexpr int SELECTION_IDS =
		(CanvasModel::MAIN_SELECTION_ID << 8) | CanvasModel::MAIN_SELECTION_ID;
	if(TransformQuad(m_srcBounds) != m_dstQuad) {
		int srcX = m_srcBounds.x();
		int srcY = m_srcBounds.y();
		int srcW = m_srcBounds.width();
		int srcH = m_srcBounds.height();
		int dstTopLeftX = qRound(m_dstQuad.topLeft().x());
		int dstTopLeftY = qRound(m_dstQuad.topLeft().y());
		bool moveContents = !m_pasted;
		bool moveSelection = !moveContents || !m_stamped;
		bool needsMask = moveContents && moveNeedsMask();
		QVector<net::Message> msgs;
		msgs.reserve(1 + (moveContents ? 1 : 0) + (moveSelection ? 1 : 0));
		if(compatibilityMode) {
			int dstTopRightX = qRound(m_dstQuad.topRight().x());
			int dstTopRightY = qRound(m_dstQuad.topRight().y());
			int dstBottomRightX = qRound(m_dstQuad.bottomRight().x());
			int dstBottomRightY = qRound(m_dstQuad.bottomRight().y());
			int dstBottomLeftX = qRound(m_dstQuad.bottomLeft().x());
			int dstBottomLeftY = qRound(m_dstQuad.bottomLeft().y());
			if(moveSelection) {
				msgs.append(net::makeMoveRegionMessage(
					contextId, 0, srcX, srcY, srcW, srcH, dstTopLeftX,
					dstTopLeftY, dstTopRightX, dstTopRightY, dstBottomRightX,
					dstBottomRightY, dstBottomLeftX, dstBottomLeftY, QImage()));
			}
			if(moveContents) {
				msgs.append(net::makeMoveRegionMessage(
					contextId, m_sourceLayerId, srcX, srcY, srcW, srcH,
					dstTopLeftX, dstTopLeftY, dstTopRightX, dstTopRightY,
					dstBottomRightX, dstBottomRightY, dstBottomLeftX,
					dstBottomLeftY,
					needsMask ? convertMaskToMono() : QImage()));
			}
		} else if(moveIsOnlyTranslated()) {
			if(moveSelection) {
				msgs.append(net::makeMoveRectMessage(
					contextId, SELECTION_IDS, 0, srcX, srcY, dstTopLeftX,
					dstTopLeftY, srcW, srcH, QImage()));
			}
			if(moveContents) {
				msgs.append(net::makeMoveRectMessage(
					contextId, layerId, m_sourceLayerId, srcX, srcY,
					dstTopLeftX, dstTopLeftY, srcW, srcH,
					needsMask ? m_mask : QImage()));
			}
		} else {
			int dstTopRightX = qRound(m_dstQuad.topRight().x());
			int dstTopRightY = qRound(m_dstQuad.topRight().y());
			int dstBottomRightX = qRound(m_dstQuad.bottomRight().x());
			int dstBottomRightY = qRound(m_dstQuad.bottomRight().y());
			int dstBottomLeftX = qRound(m_dstQuad.bottomLeft().x());
			int dstBottomLeftY = qRound(m_dstQuad.bottomLeft().y());
			if(moveSelection) {
				msgs.append(net::makeTransformRegionMessage(
					contextId, SELECTION_IDS, 0, srcX, srcY, srcW, srcH,
					dstTopLeftX, dstTopLeftY, dstTopRightX, dstTopRightY,
					dstBottomRightX, dstBottomRightY, dstBottomLeftX,
					dstBottomLeftY, getEffectiveInterpolation(interpolation),
					QImage()));
			}
			if(moveContents) {
				msgs.append(net::makeTransformRegionMessage(
					contextId, layerId, m_sourceLayerId, srcX, srcY, srcW, srcH,
					dstTopLeftX, dstTopLeftY, dstTopRightX, dstTopRightY,
					dstBottomRightX, dstBottomRightY, dstBottomLeftX,
					dstBottomLeftY, getEffectiveInterpolation(interpolation),
					needsMask ? m_mask : QImage()));
			}
		}
		if(!msgs.isEmpty() && !containsNullMessages(msgs)) {
			msgs.prepend(net::makeUndoPointMessage(contextId));
			if(m_stamped && !m_pasted) {
				m_pasted = true;
				m_mask = QImage();
				emit transformCutCleared();
			}
			return msgs;
		}
	}
	return {};
}

QVector<net::Message> TransformModel::applyFloating(
	uint8_t contextId, int layerId, int interpolation, bool compatibilityMode,
	bool stamp)
{
	Q_ASSERT(m_active);
	Q_ASSERT(m_pasted);
	QPoint offset;
	QImage transformedImage = drawdance::transformImage(
		m_image, m_dstQuad.polygon().toPolygon(),
		getEffectiveInterpolation(interpolation), &offset);
	if(transformedImage.isNull()) {
		qWarning(
			"TransformModel::applyFloating: could not transform image: %s",
			DP_error());
		return {};
	}

	QVector<net::Message> msgs;
	if(m_stamped && !stamp) {
		msgs = applyFromCanvas(
			contextId, layerId, interpolation, compatibilityMode);
	}

	// May be empty either if the transform wasn't stamped or if there was
	// nothing to modify about the selection. In both cases we still paste.
	if(msgs.isEmpty()) {
		msgs.reserve(2);
		msgs.append(net::makeUndoPointMessage(contextId));
	}

	net::makePutImageMessages(
		msgs, contextId, layerId, DP_BLEND_MODE_NORMAL, offset.x(), offset.y(),
		transformedImage);

	return msgs;
}

void TransformModel::clear()
{
	m_active = false;
	m_pasted = false;
	m_stamped = false;
	m_dstQuadValid = false;
	m_srcBounds = QRect();
	m_dstQuad.clear();
	m_mask = QImage();
	m_image = QImage();
}

bool TransformModel::moveNeedsMask() const
{
	Q_ASSERT(!m_pasted);
	Q_ASSERT(m_mask.format() == QImage::Format_ARGB32_Premultiplied);
	size_t count = size_t(m_mask.width()) * size_t(m_mask.height());
	const uint32_t *pixels = reinterpret_cast<const uint32_t *>(m_mask.bits());
	for(size_t i = 0; i < count; ++i) {
		if(qAlpha(pixels[i]) != 255) {
			return true;
		}
	}
	return false;
}

bool TransformModel::moveIsOnlyTranslated() const
{
	TransformQuad src(m_srcBounds);
	QPointF topLeftDelta = m_dstQuad.topLeft() - src.topLeft();
	for(int i = 1; i < 4; ++i) {
		QPointF delta = m_dstQuad.at(i) - src.at(i);
		if(!qFuzzyCompare(delta.x(), topLeftDelta.x()) ||
		   !qFuzzyCompare(delta.y(), topLeftDelta.y())) {
			return false;
		}
	}
	return true;
}

QImage TransformModel::convertMaskToMono() const
{
	Q_ASSERT(!m_mask.isNull());
	int width = m_mask.width();
	int height = m_mask.height();
	QImage mono(width, height, QImage::Format_Mono);
	mono.fill(0);
	const uint32_t *src =
		reinterpret_cast<const uint32_t *>(m_mask.constBits());
	for(int y = 0; y < height; ++y) {
		for(int x = 0; x < width; ++x) {
			if(qAlpha(src[y * width + x]) != 0) {
				mono.setPixel(x, y, 1);
			}
		}
	}
	return mono;
}

bool TransformModel::containsNullMessages(const QVector<net::Message> &msgs)
{
	for(const net::Message &msg : msgs) {
		if(msg.isNull()) {
			return true;
		}
	}
	return false;
}

bool TransformModel::isRightAngleRotationOrReflection(const QTransform &t) const
{
	QPoint scales[] = {
		QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1)};
	for(int angle = 0; angle <= 270; angle += 90) {
		for(const QPointF scale : scales) {
			QTransform u;
			if(angle != 0) {
				u.rotate(angle);
			}
			if(scale.x() != 1 || scale.y() != 1) {
				u.scale(scale.x(), scale.y());
			}
			if(qFuzzyCompare(t, u)) {
				return true;
			}
		}
	}
	return false;
}

bool TransformModel::isQuadValid(const TransformQuad &quad)
{
	const QPolygonF &polygon = quad.polygon();
	if(polygon.size() == 4 && !polygon.boundingRect().isEmpty()) {
		int sign = crossProductSign(polygon[0], polygon[1], polygon[2]);
		return crossProductSign(polygon[1], polygon[2], polygon[3]) == sign &&
			   crossProductSign(polygon[2], polygon[3], polygon[0]) == sign &&
			   crossProductSign(polygon[3], polygon[0], polygon[1]) == sign;
	} else {
		return false;
	}
}

int TransformModel::crossProductSign(
	const QPointF &a, const QPointF &b, const QPointF &c)
{
	qreal crossProduct =
		(b.x() - a.x()) * (c.y() - b.y()) - (b.y() - a.y()) * (c.x() - b.x());
	return crossProduct < 0.0 ? -1 : crossProduct > 0.0 ? 1 : 0;
}

}
