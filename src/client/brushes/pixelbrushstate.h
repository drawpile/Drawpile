/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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
#ifndef BRUSHES_PIXELBRUSHSTATE_H
#define BRUSHES_PIXELBRUSHSTATE_H

#include "brush.h"
#include "brushstate.h"
#include "core/point.h"
#include "../shared/net/brushes.h"

namespace paintcore {
	class LayerStack;
	class Layer;
	struct BrushStamp;
}

namespace brushes {

/**
 * @brief Pixel brush engine
 *
 * This class keeps track of a brush stroke's state and generates
 * DrawDabs commands.
 */
class PixelBrushState : public BrushState {
public:
	PixelBrushState();

	/**
	 * @brief Set the context (user) ID
	 * @param id
	 */
	void setContextId(int id) { m_contextId = id; }

	/**
	 * @brief Set the brush parameters
	 */
	void setBrush(const ClassicBrush &brush);

	/**
	 * @brief Set the target layer
	 * @param id layer ID
	 */
	void setLayer(int id) { m_layerId = id; }

	/**
	 * @brief Start or continue a stroke
	 * @param sourceLayer unused
	 */
	void strokeTo(const paintcore::Point &p, const paintcore::Layer *) override;

	/**
	 * @brief End the active stroke
	 */
	void endStroke() override;

	/**
	 * @brief Take the current DrawDab commands
	 *
	 * This clears the dab buffer but does not end the
	 * stroke.
	 *
	 * @return list of DrawDab commands generated so far
	 */
	protocol::MessageList takeDabs() override {
		auto dabs = m_dabs;
		m_dabs = protocol::MessageList();
		m_lastDab = nullptr;
		return dabs;
	}

	void addOffset(int x, int y) override;

private:
	void addDab(int x, int y, qreal pressure);

	ClassicBrush m_brush;      // the current brush
	int m_contextId;           // user context ID
	int m_layerId;             // target layer ID
	qreal m_length;            // current length of active brush stroke
	bool m_pendown;            // brush stroke in progress?
	paintcore::Point m_lastPoint;

	protocol::MessageList m_dabs;
	protocol::DrawDabsPixel *m_lastDab;
	int m_lastDabX;
	int m_lastDabY;
};

}

#endif
