/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2019 Calle Laakkonen

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
#ifndef TOOLSETTINGS_H
#define TOOLSETTINGS_H

#include "utils/icon.h"
#include "tools/tool.h"

class QComboBox;

namespace widgets { class BrushPreview; }

namespace tools {

class ToolProperties;
class ToolController;

/**
 * @brief Abstract base class for tool settings widgets
 *
 * The tool settings class provides a user interface widget that is
 * displayed in a dock window and a uniform way of getting a brush
 * configured by the user.
 */
class ToolSettings : public QObject {
	Q_OBJECT
public:
	ToolSettings(ToolController *ctrl, QObject *parent)
		: QObject(parent), m_ctrl(ctrl), m_widget(nullptr)
	{
		Q_ASSERT(ctrl);
	}

	virtual ~ToolSettings() = default;

	/**
	 * @brief Create the UI widget
	 *
	 * If the tool has a size changing signal, it will be connected to the
	 * parent's sizeChanged(int) signal.
	 *
	 * @param parent parent widget
	 * @return UI widget
	 */
	QWidget *createUi(QWidget *parent);

	//! Get the UI widget
	QWidget *getUi() { return m_widget; }

	//! Get the header bar UI widget (if any)
	virtual QWidget *getHeaderWidget() { return nullptr; }

	/**
	 * @brief Get the type of tool these settings are meant for.
	 *
	 * (Some tools, like freehand and line, share the same type)
	 */
	virtual QString toolType() const = 0;

	/**
	 * @brief Select the currently active tool
	 *
	 * Some tool settings pages support multiple tools
	 * and can adapt their features based on the selection.
	 */
	virtual void setActiveTool(tools::Tool::Type tool) { Q_UNUSED(tool); }

	//! Set the foreground color
	virtual void setForeground(const QColor& color) = 0;

	/**
	 * @brief Quick adjust a tool parameter
	 *
	 * This is a shortcut for adjusting a tool parameter.
	 * For brush based tools, this adjust the size.
	 * @param adjustment how much to adjust by (-1/1 is the normal rate)
	 */
	virtual void quickAdjust1(qreal adjustment) { Q_UNUSED(adjustment) }

	/**
	 * @brief Increase or decrease tool parameter by one "step".
	 *
	 * Unlike quickAdjust, this is a digital increase. For brush-based tools,
	 * this increases the size according to the current size.
	 *
	 * @param increase If this is an increase (true) or decrease (false).
	 */
	virtual void stepAdjust1(bool increase) { Q_UNUSED(increase); }

	/**
	 * @brief Get the current brush size
	 * @return size of the current brush
	 */
	virtual int getSize() const = 0;

	/**
	 * @brief Is this tool in subpixel precision mode
	 *
	 * This affects how the brush outline should be drawn.
	 * At integer resolution, the outline should snap to
	 * pixel edges.
	 */
	virtual bool getSubpixelMode() const = 0;

	/**
	 * @brief Does the tool need a square brush outline
	 *
	 * Default outline (if used) is circular.
	 */
	virtual bool isSquare() const { return false; }

	//! Push settings to the tool controller
	virtual void pushSettings();

	/**
	 * @brief Save the settings of this tool
	 * @return saved tool settings
	 */
	virtual ToolProperties saveToolSettings();

	/**
	 * @brief Load settings for this tool
	 * @param props
	 */
	virtual void restoreToolSettings(const ToolProperties &);

	/**
	 * @brief Step the given value logarithmically larger or smaller.
	 *
	 * Used for increasing or decreasing brush sizes and such by one "step",
	 * where that step gets larger as the brush gets larger.
	 */
	static int stepLogarithmic(
		int min, int max, int current, bool increase, double stepSize = 15.0);

	/**
	 * @brief Step the given value linearly larger or smaller.
	 *
	 * Used for increasing or decreasing MyPaint brush sizes by one step. Their
	 * size is already logarithmic, so the value change needs to be linear.
	 */
	static int stepLinear(
		int min, int max, int current, bool increase, int stepSize = 12);

public slots:
	//! Toggle tool eraser mode (if it has one)
	virtual void toggleEraserMode() { }

protected:
	virtual QWidget *createUiWidget(QWidget *parent) = 0;	
	ToolController *controller() { return m_ctrl; }

private:
	ToolController *m_ctrl;
	QWidget *m_widget;
};

}

#endif

