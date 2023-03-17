/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2015 Calle Laakkonen

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
#ifndef TOOLS_ANNOTATION_H
#define TOOLS_ANNOTATION_H

#include "libclient/tools/tool.h"

#include <QRect>

namespace tools {

/**
 * @brief Annotation tool
 */
class Annotation final : public Tool {
public:
	Annotation(ToolController &owner);

	void begin(const canvas::Point& point, bool right, float zoom) override;
	void motion(const canvas::Point& point, bool constrain, bool center) override;
	void end() override;

private:
	/// Where the annotation was grabbed
	enum class Handle {
		Outside,
		Inside,
		TopLeft,
		TopRight,
		BottomRight,
		BottomLeft,
		Top,
		Right,
		Bottom,
		Left
	};

	/// ID of the currently selected annotation
	uint16_t m_selectedId;

	/// Are we currently creating a new annotation?
	bool m_isNew;

	/// Drag start and last point
	QPointF m_p1, m_p2;

	/// Grabbed handle
	Handle m_handle;

	/// The shape of the annotation being edited
	QRect m_shape;

	Handle handleAt(const QRect &rect, const QPoint &p, int handleSize);
};

}

#endif

