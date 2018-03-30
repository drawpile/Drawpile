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
#ifndef BRUSHES_BRUSHSTATE_H
#define BRUSHES_BRUSHSTATE_H

//#include "brush.h"
#include "core/brush.h"
#include "core/point.h"
#include "../shared/net/brushes.h"

#include <QPointer>

namespace paintcore {
	class LayerStack;
	class Layer;
	class BrushStamp;
}

namespace brushes {

/**
 * @brief Drawpile's classic brush engine
 *
 * This class keeps track of a brush stroke's state and generates
 * DrawDabs commands.
 */
class ClassicBrushState {
public:
	ClassicBrushState();

	/**
	 * @brief Set the context (user) ID
	 * @param id
	 */
	void setContextId(int id) { m_contextId = id; }

	/**
	 * @brief Set the brush parameters
	 */
	void setBrush(const paintcore::Brush &brush);

	/**
	 * @brief Set the target layer
	 * @param id layer ID
	 */
	void setLayer(int id) { m_layerId = id; }

	/**
	 * @brief Start or continue a stroke
	 * @param sourceLayer layer to pick up color from (when smudging)
	 */
	void strokeTo(const paintcore::Point &p, const paintcore::Layer *sourceLayer);

	/**
	 * @brief End the active stroke
	 */
	void endStroke();

	/**
	 * @brief Take the current DrawDab commands
	 *
	 * This clears the dab buffer but does not end the
	 * stroke.
	 *
	 * @return list of DrawDab commands generated so far
	 */
	QList<protocol::MessagePtr> takeDabs() {
		auto dabs = m_dabs;
		m_dabs = QList<protocol::MessagePtr>();
		m_lastDab = nullptr;
		return dabs;
	}

private:
	void addDab(const paintcore::Point &point, quint32 color);

	paintcore::Brush m_brush;  // the current brush
	int m_contextId;           // user context ID
	int m_layerId;             // target layer ID
	qreal m_length;            // current length of active brush stroke
	int m_smudgeDistance;      // dabs since last smudge color sampling
	QColor m_smudgedColor;     // effective color (nonzero alpha indicates indirect drawing mode)
	bool m_pendown;            // brush stroke in progress?
	paintcore::Point m_lastPoint;

	QList<protocol::MessagePtr> m_dabs;
	protocol::DrawDabsClassic *m_lastDab;
	int m_lastDabX;
	int m_lastDabY;
};

}

#endif



