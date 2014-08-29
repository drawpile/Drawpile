/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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
#ifndef TOOLS_SHAPETOOLS_H
#define TOOLS_SHAPETOOLS_H

#include "tool.h"

namespace tools {

/**
 * \brief Line tool
 *
 * The line tool draws straight lines.
 */
class Line : public Tool {
public:
	Line(ToolCollection &owner) : Tool(owner, LINE) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

private:
	QPointF _p1, _p2;
	bool _swap;
};

/**
 * \brief Base class for shape drawing tools that can be defined with a rectangle
 */
class RectangularTool : public Tool {
public:
	RectangularTool(ToolCollection &owner, Type type, QCursor cursor) : Tool(owner, type, cursor) {}

	void begin(const paintcore::Point& point, bool right);
	void motion(const paintcore::Point& point, bool constrain, bool center);
	void end();

protected:
	virtual QAbstractGraphicsShapeItem *createPreview(const paintcore::Point &p) = 0;
	virtual void updateToolPreview() = 0;
	virtual paintcore::PointVector pointVector() = 0;
	QRectF rect() const { return QRectF(_p1, _p2).normalized(); }

private:
	QPointF _start, _p1, _p2;
	bool _swap;
};

/**
 * \brief Rectangle drawing tool
 *
 * This tool is used for drawing squares and rectangles
 */
class Rectangle : public RectangularTool {
public:
	Rectangle(ToolCollection &owner);

protected:
	virtual QAbstractGraphicsShapeItem *createPreview(const paintcore::Point &p);
	virtual void updateToolPreview();
	virtual paintcore::PointVector pointVector();
};

/**
 * \brief Ellipse drawing tool
 *
 * This tool is used for drawing circles and ellipses
 */
class Ellipse : public RectangularTool {
public:
	Ellipse(ToolCollection &owner);

protected:
	virtual QAbstractGraphicsShapeItem *createPreview(const paintcore::Point &p);
	virtual void updateToolPreview();
	virtual paintcore::PointVector pointVector();
};

}

#endif

