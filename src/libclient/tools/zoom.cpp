/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#include "canvas/canvasmodel.h"
#include "canvas/selection.h"

#include "tools/zoom.h"
#include "tools/toolcontroller.h"

#include <QCursor>
#include <QPixmap>

namespace tools {

ZoomTool::ZoomTool(ToolController &owner)
	: Tool(owner, ZOOM, QCursor(QPixmap(":cursors/zoom.png"), 8, 8), true, false, true)
{ }

void ZoomTool::begin(const canvas::Point &point, bool right, float zoom)
{
	Q_UNUSED(zoom);
	m_start = point.toPoint();
	m_end = m_start;
	m_reverse = right;

	// We use the selection item to preview the zoom rectangle
	// This is OK, since the zoom rectangle is visible only when
	// the zoom tool is active, and both tools can't be active at the same time
	auto sel = new canvas::Selection;
	sel->setShapeRect(QRect(m_start, m_start));
	owner.model()->setSelection(sel);
}

void ZoomTool::motion(const canvas::Point &point, bool constrain, bool center)
{
	Q_UNUSED(constrain);
	Q_UNUSED(center);

	auto sel = owner.model()->selection();
	if(!sel) {
		qWarning("ZoomTool::motion(): no selection!");
		return;
	}

	m_end = point.toPoint();
	sel->setShapeRect(QRect(m_start, m_end).normalized());
}

void ZoomTool::end()
{
	auto sel = owner.model()->selection();
	if(!sel)
		return;

	const int steps = 3;

	emit owner.zoomRequested(sel->boundingRect(), m_reverse ? -steps : steps);

	owner.model()->setSelection(nullptr);
}

}

