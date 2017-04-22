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

#include "core/annotationmodel.h"
#include "tool.h"

namespace drawingboard {
	class AnnotationItem;
}

namespace tools {

/**
 * @brief Annotation tool
 */
class Annotation : public Tool {
public:
	Annotation(ToolController &owner);

	void begin(const paintcore::Point& point, bool right, float zoom) override;
	void motion(const paintcore::Point& point, bool constrain, bool center) override;
	void end() override;

private:
	static const int PREVIEW_ID = 0x010000;
	int m_selectedId;
	bool m_isNew;

	QPointF m_p1, m_p2;
	paintcore::Annotation::Handle m_handle;
};

}

#endif

