/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2017 Calle Laakkonen

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
#ifndef TOOLS_SELECTION_H
#define TOOLS_SELECTION_H

#include "canvas/selection.h"
#include "tool.h"

class QImage;
class QPolygon;

namespace tools {

//! Base class for selection tools
class SelectionTool : public Tool {
public:
	SelectionTool(ToolController &owner, Type type, QCursor cursor)
		: Tool(owner, type,  cursor) { }

	void begin(const paintcore::Point& point, bool right, float zoom) override;
	void motion(const paintcore::Point& point, bool constrain, bool center) override;
	void end() override;

	void finishMultipart() override;
	void cancelMultipart() override;
	bool isMultipart() const override;

	//! Start a layer region move operation
	void startMove();

	static QImage transformSelectionImage(const QImage &source, const QPolygon &target, QPoint *offset);
	static QImage shapeMask(const QColor &color, const QPolygonF &selection, QRect *maskBounds, bool mono=false);

protected:
	virtual void initSelection(canvas::Selection *selection) = 0;
	virtual void newSelectionMotion(const paintcore::Point &point, bool constrain, bool center) = 0;

	QPointF m_start, m_p1;
	canvas::Selection::Handle m_handle;
};

/**
 * @brief Selection tool
 *
 * This is used for selecting regions for copying & pasting.
 */
class RectangleSelection : public SelectionTool {
public:
	RectangleSelection(ToolController &owner);

protected:
	void initSelection(canvas::Selection *selection);
	void newSelectionMotion(const paintcore::Point &point, bool constrain, bool center);
};

class PolygonSelection : public SelectionTool {
public:
	PolygonSelection(ToolController &owner);

	void end();

protected:
	void initSelection(canvas::Selection *selection);
	void newSelectionMotion(const paintcore::Point &point, bool constrain, bool center);
};

}

#endif

