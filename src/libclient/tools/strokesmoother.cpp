// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/tools/strokesmoother.h"
#include "libclient/settings.h"

StrokeSmoother::StrokeSmoother()
{
	reset();
}

bool StrokeSmoother::isActive() const
{
	return m_points.size() > 0;
}

void StrokeSmoother::setSmoothing(int strength)
{
	m_points.resize(qBound(0, strength, libclient::settings::maxSmoothing));
	reset();
}

void StrokeSmoother::addOffset(const QPointF &offset)
{
	for(int i=0;i<m_points.size();++i)
		m_points[i] += offset;
}

void StrokeSmoother::addPoint(const canvas::Point &point)
{
	Q_ASSERT(m_points.size()>0);

	if(m_count == 0) {
		/* Pad the buffer with this point, so we blend away from it
		 * gradually as we gain more points. We still only count this
		 * as one point so we know how much real data we have to
		 * drain if it was a very short stroke. */
		m_points.fill(point);
	} else {
		if(--m_pos < 0)
			m_pos = m_points.size()-1;
		m_points[m_pos] = point;
	}

	if(m_count < m_points.size())
		++m_count;
}

canvas::Point StrokeSmoother::at(int i) const
{
	return m_points.at((m_pos+i) % m_points.size());
}

void StrokeSmoother::reset()
{
	m_count=0;
	m_pos = 0;
}

bool StrokeSmoother::hasSmoothPoint() const
{
	return m_count > 0;
}

canvas::Point StrokeSmoother::smoothPoint() const
{
	Q_ASSERT(hasSmoothPoint());

	// A simple unweighted sliding-average smoother
	auto p = at(0);

	qreal pressure = p.pressure();
	qreal xtilt = p.xtilt();
	qreal ytilt = p.ytilt();
	qreal rotation = p.rotation();
	qreal timeMsec = p.timeMsec();
	for(int i=1;i<m_points.size();++i) {
		const auto pi = at(i);
		p.rx() += pi.x();
		p.ry() += pi.y();
		pressure += pi.pressure();
		xtilt += pi.xtilt();
		ytilt += pi.ytilt();
		rotation += pi.rotation();
		timeMsec += pi.timeMsec();
	}

	const qreal c = m_points.size();
	p.rx() /= c;
	p.ry() /= c;
	p.setPressure(pressure / c);
	p.setXtilt(xtilt / c);
	p.setYtilt(ytilt / c);
	p.setRotation(rotation / c);
	p.setTimeMsec(timeMsec / c + 0.5);

	return p;
}

void StrokeSmoother::removePoint()
{
	Q_ASSERT(!m_points.isEmpty());
	/* Pad the buffer with the final point, overwriting the oldest first,
	 * for symmetry with starting. For very short strokes this should
	 * really set all points between --_count and _points.size()-1. */
	m_points[(m_pos + --m_count) % m_points.size()] = latestPoint();
}
