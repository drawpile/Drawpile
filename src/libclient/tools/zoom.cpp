// SPDX-License-Identifier: GPL-3.0-or-later
#include "libclient/tools/zoom.h"
#include "libclient/tools/toolcontroller.h"
#include "libclient/utils/cursors.h"
#include <QPainterPath>

namespace tools {

ZoomTool::ZoomTool(ToolController &owner)
	: Tool(
		  owner, ZOOM, utils::Cursors::zoom(),
		  Capability::AllowColorPick | Capability::HandlesRightClick |
			  Capability::SendsNoMessages)
{
}

void ZoomTool::begin(const BeginParams &params)
{
	m_clickDetector.begin(params.viewPos, params.deviceType);
	m_drawing = true;
	m_reverse = params.right ^ params.constrain;
	m_scrubbing = m_reverse ? !m_scrub : m_scrub;
	m_scrubPos = params.point;
	m_scrubLast = params.viewPos;
	m_start = params.point.toPoint();
	m_end = m_start;
}

void ZoomTool::motion(const MotionParams &params)
{
	m_clickDetector.motion(params.viewPos);
	m_end = params.point.toPoint();
	if(m_scrubbing) {
		QPointF viewPos = params.viewPos;
		qreal delta = params.center ? viewPos.x() - m_scrubLast.x()
									: m_scrubLast.y() - viewPos.y();
		Q_EMIT m_owner.scrubZoomRequested(delta, m_scrubPos);
		m_scrubLast = viewPos;
	} else {
		updatePreview(params.center);
	}
}

void ZoomTool::modify(const ModifyParams &params)
{
	if(m_drawing && !m_scrubbing) {
		updatePreview(params.center);
	}
}

void ZoomTool::end(const EndParams &params)
{
	if(m_drawing) {
		m_clickDetector.end();
		if(!m_scrubbing) {
			removePreview();
		}

		if(m_clickDetector.isClick()) {
			emit m_owner.zoomStepsRequested(
				m_reverse ? -STEPS : STEPS,
				QPointF(getRect(params.center).center()));
		} else if(!m_scrubbing) {
			QRect rect = getRect(params.center);
			if(rect.width() >= MIN_DIMENSION &&
			   rect.height() >= MIN_DIMENSION) {
				emit m_owner.zoomToRectRequested(rect);
			}
		}

		m_drawing = false;
		m_scrubbing = false;
	}
}

void ZoomTool::setScrub(bool scrub)
{
	if(scrub != m_scrub) {
		m_scrub = scrub;
		setCapability(Capability::Fractional, scrub);
	}
}

void ZoomTool::updatePreview(bool center) const
{
	QRect rect = getRect(center);
	if(rect.isEmpty()) {
		removePreview();
	} else {
		QPainterPath path;
		path.addRect(rect);
		emit m_owner.pathPreviewRequested(path);
	}
}

void ZoomTool::removePreview() const
{
	Q_EMIT m_owner.pathPreviewRequested(QPainterPath());
}

QRect ZoomTool::getRect(bool center) const
{
	if(m_start == m_end) {
		return QRect(m_start, m_end);
	} else {
		QPoint p1, p2;
		if(center) {
			p1 = m_end;
			p2 = m_start - (m_end - m_start);
		} else {
			p1 = m_start;
			p2 = m_end;
		}
		return QRect(
			QPoint(qMin(p1.x(), p2.x()), qMin(p1.y(), p2.y())),
			QPoint(qMax(p1.x(), p2.x()), qMax(p1.y(), p2.y())));
	}
}

}
