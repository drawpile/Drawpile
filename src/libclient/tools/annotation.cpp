/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2021 Calle Laakkonen

   Drawpile is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Drawpile is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "libclient/canvas/canvasmodel.h"
#include "libclient/canvas/paintengine.h"
#include "libclient/net/client.h"
#include "libclient/drawdance/message.h"

#include "libclient/tools/toolcontroller.h"
#include "libclient/tools/annotation.h"

#include <QPixmap>

namespace tools {

Annotation::Annotation(ToolController &owner)
	: Tool(owner, ANNOTATION, QCursor(QPixmap(":cursors/text.png"), 2, 2), false, false, true)
{
}

/**
 * The annotation tool has fairly complex needs. Clicking on an existing
 * annotation selects it, otherwise a new annotation is started.
 */
void Annotation::begin(const canvas::Point& point, bool right, float zoom)
{
	if(right) {
		m_selectedId = 0;
		owner.setActiveAnnotation(m_selectedId);
		return;
	}

	m_p1 = point;
	m_p2 = point;

	const int handleSize = qRound(qMax(10.0, 10.0 / zoom) / 2.0);
	drawdance::Annotation selection = owner.model()->paintEngine()->getAnnotationAt(point.x(), point.y(), handleSize);

	if(selection.isNull()) {
		// No annotation, start creating a new one
		if(!owner.model()->aclState()->canUseFeature(DP_FEATURE_CREATE_ANNOTATION)) {
			m_handle = Handle::Outside;
			return;
		}

		m_selectedId = owner.model()->paintEngine()->findAvailableAnnotationId(owner.model()->localUserId());
		m_handle = Handle::BottomRight;
		m_shape = QRect { m_p1.toPoint(), QSize{1, 1} };
		m_isNew = true;

		// Note: The tool functions perfectly even if nothing happens in
		// response to the call, only the visual feedback will be missing.
		if(m_selectedId > 0)
			owner.model()->previewAnnotation(m_selectedId, m_shape);
	} else {
		m_isNew = false;
		m_selectedId = selection.id();
		m_shape = selection.bounds();

		if(selection.protect() && !owner.model()->aclState()->amOperator() && (m_selectedId >> 8) != owner.client()->myId()) {
			m_handle = Handle::Outside;
		} else {
			m_handle = handleAt(m_shape, point.toPoint(), handleSize);
		}

		owner.setActiveAnnotation(m_selectedId);
	}
}

Annotation::Handle Annotation::handleAt(const QRect &rect, const QPoint &point, int handleSize)
{
	const QRect R {
		rect.x()-handleSize/2,
		rect.y()-handleSize/2,
		rect.width()+handleSize,
		rect.height()+handleSize
	};

	const QPointF p = point - R.topLeft();

	if(p.x() < handleSize) {
		if(p.y() < handleSize)
			return Handle::TopLeft;
		else if(p.y() > R.height()-handleSize)
			return Handle::BottomLeft;
		return Handle::Left;
	} else if(p.x() > R.width() - handleSize) {
		if(p.y() < handleSize)
			return Handle::TopRight;
		else if(p.y() > R.height() - handleSize)
			return Handle::BottomRight;
		return Handle::Right;
	} else if(p.y() < handleSize)
		return Handle::Top;
	else if(p.y() > R.height() - handleSize)
		return Handle::Bottom;

	return Handle::Inside;
}

/**
 * Change the shape of the selected annotation.
 */
void Annotation::motion(const canvas::Point& point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);
	if(m_selectedId == 0)
		return;

	const QPoint delta = (point - m_p2).toPoint();
	if(delta.manhattanLength()==0)
		return;

	m_p2 = point;

	switch(m_handle) {
	case Handle::Outside: return;
	case Handle::Inside: m_shape.translate(delta); break;
	case Handle::TopLeft: m_shape.adjust(delta.x(), delta.y(), 0, 0); break;
	case Handle::TopRight: m_shape.adjust(0, delta.y(), delta.x(), 0); break;
	case Handle::BottomRight: m_shape.adjust(0, 0, delta.x(), delta.y()); break;
	case Handle::BottomLeft: m_shape.adjust(delta.x(), 0, 0, delta.y()); break;
	case Handle::Top: m_shape.adjust(0, delta.y(), 0, 0); break;
	case Handle::Right: m_shape.adjust(0, 0, delta.x(), 0); break;
	case Handle::Bottom: m_shape.adjust(0, 0, 0, delta.y()); break;
	case Handle::Left: m_shape.adjust(delta.x(), 0, 0, 0); break;
	}

	if(m_shape.left() > m_shape.right() || m_shape.top() > m_shape.bottom()) {
		if(m_shape.left() > m_shape.right()) {
			switch(m_handle) {
			case Handle::TopLeft: m_handle = Handle::TopRight; break;
			case Handle::TopRight: m_handle = Handle::TopLeft; break;
			case Handle::BottomRight: m_handle = Handle::BottomLeft; break;
			case Handle::BottomLeft: m_handle = Handle::BottomRight; break;
			case Handle::Left: m_handle = Handle::Right; break;
			case Handle::Right: m_handle = Handle::Left; break;
			default: break;
			}
		}
		if(m_shape.top() > m_shape.bottom()) {
			switch(m_handle) {
			case Handle::TopLeft: m_handle = Handle::BottomLeft; break;
			case Handle::TopRight: m_handle = Handle::BottomRight; break;
			case Handle::BottomRight: m_handle = Handle::TopRight; break;
			case Handle::BottomLeft: m_handle = Handle::TopRight; break;
			case Handle::Top: m_handle = Handle::Bottom; break;
			case Handle::Bottom: m_handle = Handle::Top; break;
			default: break;
			}
		}

		m_shape = m_shape.normalized();
	}

	owner.model()->previewAnnotation(m_selectedId, m_shape);
}

/**
 * If we have a selected annotation, adjust its shape.
 * Otherwise, create a new annotation.
 */
void Annotation::end()
{
	if(m_selectedId == 0)
		return;

	const uint8_t contextId = owner.client()->myId();
	drawdance::Message msg;

	if(!m_isNew) {
		if(m_p1.toPoint() != m_p2.toPoint()) {
			msg = drawdance::Message::noinc(DP_msg_annotation_reshape_new(
				contextId, m_selectedId, m_shape.x(), m_shape.y(), m_shape.width(), m_shape.height()));
		}
	} else if(m_handle != Handle::Outside) {
		if(m_shape.width() < 10 && m_shape.height() < 10) {
			// User created a tiny annotation, probably by clicking rather than dragging.
			// Create a nice and big annotation box rather than a minimum size one.
			m_shape.setSize(QSize(160, 60));
		}
		msg = drawdance::Message::noinc(DP_msg_annotation_create_new(
			contextId, m_selectedId, m_shape.x(), m_shape.y(), m_shape.width(), m_shape.height()));
	}

	if(!msg.isNull()) {
		drawdance::Message messages[] = {
			drawdance::Message::noinc(DP_msg_undo_point_new(contextId)),
			msg,
		};
		owner.client()->sendMessages(DP_ARRAY_LENGTH(messages), messages);
	}

	m_selectedId = 0;
}

}
