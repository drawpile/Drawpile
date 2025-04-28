// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_TOOLS_SHAPETOOLS_H
#define LIBCLIENT_TOOLS_SHAPETOOLS_H
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
		: Tool(
			  owner, type, cursor,
			  Capability::AllowColorPick | Capability::AllowToolAdjust |
				  Capability::Fractional)
	{
	}

	void begin(const BeginParams &params) final override;
	void motion(const MotionParams &params) final override;
	void modify(const ModifyParams &params) final override;
	void end(const EndParams &params) final override;

	void cancelMultipart() final override;
	bool usesBrushColor() const final override { return true; }
	void setBrushSizeLimit(int limit) override;

protected:
	virtual canvas::PointVector pointVector() const = 0;
	virtual QPointF getConstrainPoint() const;
	void updatePreview();
	QRectF rect() const { return QRectF(m_p1, m_p2).normalized(); }

	QPointF m_start, m_current, m_p1, m_p2;
	bool m_drawing = false;

private:
	void updateShape(bool constrain, bool center);

	qreal m_zoom = 1.0;
	qreal m_angle = 0.0;
	bool m_mirror = false;
	bool m_flip = false;
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

protected:
	canvas::PointVector pointVector() const override;
	QPointF getConstrainPoint() const override;
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
