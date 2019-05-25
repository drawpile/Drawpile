/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018-2019 Calle Laakkonen

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

#include "classicbrushstate.h"
#include "core/layerstack.h"
#include "core/brushmask.h"
#include "core/layer.h"

namespace brushes {

ClassicBrushState::ClassicBrushState()
	: m_contextId(0), m_layerId(0),
	  m_length(0), m_smudgeDistance(0), m_pendown(false),
	  m_lastDab(nullptr), m_lastDabX(0), m_lastDabY(0)
{
}

void ClassicBrushState::setBrush(const ClassicBrush &brush)
{
	m_brush = brush;

	QColor c = m_brush.color();
	if(brush.incremental()) {
		c.setAlpha(0);

	} else {
		// If brush alpha is nonzero, indirect drawing mode
		// is used and the alpha is used as the overall transparency
		// of the entire stroke.
		c.setAlphaF(m_brush.opacity1());

		m_brush.setOpacity(1.0);
		m_brush.setOpacity2(brush.isOpacityVariable() ? 0.0 : 1.0);
	}
	m_brush.setColor(c);
	m_smudgedColor = c;

	if(m_pendown)
		qWarning("Brush changed mid-stroke!");
}

void ClassicBrushState::addOffset(int x, int y)
{
	if(m_pendown) {
		m_lastPoint += QPointF(x, y);
	}
}

void ClassicBrushState::strokeTo(const paintcore::Point &to, const paintcore::Layer *sourceLayer)
{
	if(m_pendown) {
		// Stroke in progress: draw a line
		qreal dx = to.x() - m_lastPoint.x();
		qreal dy = to.y() - m_lastPoint.y();
		const qreal dist = hypot(dx, dy);
		dx = dx / dist;
		dy = dy / dist;
		const qreal dp = (to.pressure() - m_lastPoint.pressure()) / dist;

		const qreal spacing0 = qMax(1.0, m_brush.spacingDist(m_lastPoint.pressure()));
		qreal i;
		if(m_length>=spacing0)
			i = 0;
		else if(m_length==0)
			i = spacing0;
		else
			i = m_length;

		paintcore::Point p(m_lastPoint.x() + dx*i, m_lastPoint.y() + dy*i, qBound(0.0, m_lastPoint.pressure() + dp*i, 1.0));

		while(i<=dist) {
			const qreal spacing = qMax(1.0, m_brush.spacingDist(p.pressure()));
			const qreal smudge = m_brush.smudge(p.pressure());

			if(++m_smudgeDistance > m_brush.resmudge() && smudge>0 && sourceLayer) {
				const QColor sampled = sourceLayer->colorAt(p.x(), p.y(), qRound(m_brush.size(p.pressure())));

				if(sampled.isValid()) {
					const qreal a = sampled.alphaF() * smudge;

					m_smudgedColor = QColor::fromRgbF(
						m_smudgedColor.redF() * (1-a) + sampled.redF() * a,
						m_smudgedColor.greenF() * (1-a) + sampled.greenF() * a,
						m_smudgedColor.blueF() * (1-a) + sampled.blueF() * a,
						0
					);
				}

				m_smudgeDistance = 0;
			}

			if(m_smudgedColor.isValid())
				addDab(p, m_smudgedColor.rgba());

			p.rx() += dx * spacing;
			p.ry() += dy * spacing;
			p.setPressure(qBound(0.0, p.pressure() + dp * spacing, 1.0));
			i += spacing;
		}
		m_length = i-dist;

	} else {
		// Start a new stroke
		m_pendown = true;
		if(m_brush.isColorPickMode() && sourceLayer && !m_brush.isEraser()) {
			m_smudgedColor =  sourceLayer->colorAt(to.x(), to.y(), qRound(m_brush.size(to.pressure())));
		}

		if(m_smudgedColor.isValid())
			addDab(to, m_smudgedColor.rgba());
	}

	m_lastPoint = to;
}

void ClassicBrushState::addDab(const paintcore::Point &point, quint32 color)
{
	const int x = point.x() * 4;
	const int y = point.y() * 4;
	const int opacity = m_brush.opacity(point.pressure()) * 255;

	if(opacity == 0)
		return;

	if(!m_lastDab
			|| m_lastDab->color() != color
			|| qAbs(x - m_lastDabX) > protocol::ClassicBrushDab::MAX_XY_DELTA
			|| qAbs(y - m_lastDabY) > protocol::ClassicBrushDab::MAX_XY_DELTA
			|| m_lastDab->dabs().size() >= protocol::DrawDabsClassic::MAX_DABS
	) {
		m_lastDab = new protocol::DrawDabsClassic(
			m_contextId,
			m_layerId,
			x,
			y,
			color,
			m_brush.blendingMode()
		);
		m_dabs << protocol::MessagePtr(m_lastDab);
		m_lastDabX = x;
		m_lastDabY = y;
	}

	m_lastDab->dabs() << protocol::ClassicBrushDab {
		static_cast<decltype(protocol::ClassicBrushDab::x)>(x - m_lastDabX),
		static_cast<decltype(protocol::ClassicBrushDab::y)>(y - m_lastDabY),
		static_cast<decltype(protocol::ClassicBrushDab::size)>(m_brush.size(point.pressure()) * 256),
		static_cast<decltype(protocol::ClassicBrushDab::hardness)>(m_brush.hardness(point.pressure()) * 255),
		static_cast<decltype(protocol::ClassicBrushDab::opacity)>(opacity)
	};

	m_lastDabX = x;
	m_lastDabY = y;
}

void ClassicBrushState::endStroke()
{
	m_pendown = false;
	m_length = 0;
	m_smudgeDistance = 0;
	m_smudgedColor = m_brush.color();
}

}
