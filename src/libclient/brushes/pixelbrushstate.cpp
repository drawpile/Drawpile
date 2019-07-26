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

#include "pixelbrushstate.h"
#include "core/layerstack.h"
#include "core/brushmask.h"
#include "core/layer.h"

#include <QtMath>

namespace brushes {

PixelBrushState::PixelBrushState()
	: m_contextId(0), m_layerId(0),
	  m_length(0), m_smudgeDistance(0), m_pendown(false),
	  m_lastDab(nullptr), m_lastDabX(0), m_lastDabY(0)
{
}

void PixelBrushState::setBrush(const ClassicBrush &brush)
{
	m_brush = brush;

	QColor c = m_brush.color();
	if(brush.incremental() || brush.smudge1() > 0.0) {
		// Note: when smudging is used, we must use incremental mode
		// because we currently do not sample colors from sublayers.
		c.setAlpha(0);

	} else {
		// If brush alpha is nonzero, indirect drawing mode
		// is used and the alpha is used as the overall transparency
		// of the entire stroke.
		// TODO this doesn't work right. We should use alpha-darken mode
		// and set the opacity range properly

		c.setAlphaF(m_brush.opacity1());

		m_brush.setOpacity(1.0);
		if(brush.useOpacityPressure())
			m_brush.setOpacity2(0.0);
	}
	m_brush.setColor(c);
	m_smudgedColor = c;

	if(m_pendown)
		qWarning("Brush changed mid-stroke!");
}

void PixelBrushState::addOffset(int x, int y)
{
	m_lastPoint += QPointF(x, y);
}
void PixelBrushState::strokeTo(const paintcore::Point &to, const paintcore::Layer *sourceLayer)
{
	if(m_pendown) {
		// Stroke in progress: draw a line

		const qreal dp = (to.pressure()-m_lastPoint.pressure()) / hypot(to.x()-m_lastPoint.x(), to.y()-m_lastPoint.y());

		int x0 = qFloor(m_lastPoint.x());
		int y0 = qFloor(m_lastPoint.y());
		qreal p = m_lastPoint.pressure();
		int x1 = qFloor(to.x());
		int y1 = qFloor(to.y());
		int dy = y1 - y0;
		int dx = x1 - x0;
		int stepx, stepy;

		if (dy < 0) {
			dy = -dy;
			stepy = -1;
		} else {
			stepy = 1;
		}
		if (dx < 0) {
			dx = -dx;
			stepx = -1;
		} else {
			stepx = 1;
		}

		dy *= 2;
		dx *= 2;

		qreal distance = m_length;

		if (dx > dy) {
			int fraction = dy - (dx >> 1);
			while (x0 != x1) {
				const qreal spacing = m_brush.spacingDist(p);
				if (fraction >= 0) {
					y0 += stepy;
					fraction -= dx;
				}
				x0 += stepx;
				fraction += dy;
				if(++distance >= spacing) {
					addDab(x0, y0, p, sourceLayer);
					distance = 0;
				}
				p += dp;
			}
		} else {
			int fraction = dx - (dy >> 1);
			while (y0 != y1) {
				const qreal spacing = m_brush.spacingDist(p);
				if (fraction >= 0) {
					x0 += stepx;
					fraction -= dy;
				}
				y0 += stepy;
				fraction += dx;
				if(++distance >= spacing) {
					addDab(x0, y0, p, sourceLayer);
					distance = 0;
				}
				p += dp;
			}
		}

		m_length = distance;

	} else {
		// Start a new stroke
		m_pendown = true;
		if(m_brush.isColorPickMode() && sourceLayer && m_brush.blendingMode() != paintcore::BlendMode::MODE_ERASE) {
			m_smudgedColor = sourceLayer->colorAt(to.x(), to.y(), qRound(m_brush.size(to.pressure())));
			m_smudgeDistance = -1;
		}
		addDab(to.x(), to.y(), to.pressure(), sourceLayer);
	}

	m_lastPoint = to;
}

void PixelBrushState::addDab(int x, int y, qreal pressure, const paintcore::Layer *sourceLayer)
{
	const qreal smudge = m_brush.smudge(pressure);
	const int brushSize = m_brush.size(pressure);

	if(++m_smudgeDistance > m_brush.resmudge() && smudge > 0 && sourceLayer) {
		const QColor sampled = sourceLayer->colorAt(x, y, brushSize);

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

	if(!m_smudgedColor.isValid())
		return;

	const quint32 color = m_smudgedColor.rgba();

	const int opacity = m_brush.opacity(pressure) * 255;
	if(opacity == 0)
		return;

	if(!m_lastDab
			|| qAbs(x - m_lastDabX) > protocol::PixelBrushDab::MAX_XY_DELTA
			|| qAbs(y - m_lastDabY) > protocol::PixelBrushDab::MAX_XY_DELTA
			|| m_lastDab->dabs().size() >= protocol::DrawDabsPixel::MAX_DABS
			|| m_lastDab->color() != color
	) {
		m_lastDab = new protocol::DrawDabsPixel(
			m_brush.shape() == ClassicBrush::SQUARE_PIXEL ? protocol::DabShape::Square : protocol::DabShape::Round,
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

	m_lastDab->dabs() << protocol::PixelBrushDab {
		static_cast<decltype(protocol::PixelBrushDab::x)>(x - m_lastDabX),
		static_cast<decltype(protocol::PixelBrushDab::y)>(y - m_lastDabY),
		static_cast<decltype(protocol::PixelBrushDab::size)>(brushSize),
		static_cast<decltype(protocol::PixelBrushDab::opacity)>(opacity)
	};

	m_lastDabX = x;
	m_lastDabY = y;
}

void PixelBrushState::endStroke()
{
	m_pendown = false;
	m_length = 0;
	m_smudgeDistance = 0;
	m_smudgedColor = m_brush.color();
}

}
