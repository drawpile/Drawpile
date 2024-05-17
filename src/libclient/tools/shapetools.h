// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TOOLS_SHAPETOOLS_H
#define TOOLS_SHAPETOOLS_H

#include "libclient/drawdance/brushengine.h"
#include "libclient/tools/tool.h"

#include <QRectF>

namespace tools {

/**
 * \brief Base class for tools that draw a shape (as opposed to freehand tools)
 */
class ShapeTool : public Tool {
public:
	ShapeTool(ToolController &owner, Type type, QCursor cursor)
		: Tool{owner, type, cursor, true, true, false, true, true}
		, m_drawing{false}
		, m_brushEngine{}
	{
	}

	void begin(const BeginParams &params) final override;
	void motion(const MotionParams &params) override;
	void end() final override;

	void cancelMultipart() final override;
	bool usesBrushColor() const final override { return true; }

protected:
	virtual canvas::PointVector pointVector() const = 0;
	void updatePreview();
	QRectF rect() const { return QRectF(m_p1, m_p2).normalized(); }

	QPointF m_start, m_p1, m_p2;
	bool m_drawing;

private:
	qreal m_zoom;
	drawdance::BrushEngine m_brushEngine;
};

/**
 * \brief Line tool
 *
 * The line tool draws straight lines.
 */
class Line final : public ShapeTool {
public:
	Line(ToolController &owner);

	void motion(const MotionParams &params) override;

protected:
	canvas::PointVector pointVector() const override;
};

/**
 * \brief Rectangle drawing tool
 *
 * This tool is used for drawing squares and rectangles
 */
class Rectangle final : public ShapeTool {
public:
	Rectangle(ToolController &owner);

protected:
	canvas::PointVector pointVector() const override;
};

/**
 * \brief Ellipse drawing tool
 *
 * This tool is used for drawing circles and ellipses
 */
class Ellipse final : public ShapeTool {
public:
	Ellipse(ToolController &owner);

protected:
	canvas::PointVector pointVector() const override;
};

}

#endif
