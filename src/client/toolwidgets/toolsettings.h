/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2006-2016 Calle Laakkonen

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

#include "core/brush.h"
#include "utils/icon.h"

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
class ToolSettings {
public:
	ToolSettings(const QString &name, const QString &title, const QString &icon, ToolController *ctrl)
		: m_ctrl(ctrl), m_name(name), m_title(title), m_icon(icon), m_widget(nullptr)
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

	//! Set the foreground color
	virtual void setForeground(const QColor& color) = 0;

	/**
	 * @brief Quick adjust a tool parameter
	 *
	 * This is a shortcut for adjusting a tool parameter.
	 * For brush based tools, this adjust the size.
	 * @param adjustment how much to adjust by (-1/1 is the normal rate)
	 */
	virtual void quickAdjust1(float adjustment) = 0;

	/**
	 * @brief Get the current brush size
	 * @return size of the current brush
	 */
	virtual int getSize() const = 0;

	/**
	 * @brief Is this tool in subpixel precision mode
	 */
	virtual bool getSubpixelMode() const = 0;

	/**
	 * @brief Get the internal name of this tool
	 * The internal name is used when settings are stored to a
	 * configuration file
	 * @return internal tool name
	 */
	const QString& getName() const { return m_name; }

	/**
	 * @brief Get the user facing name of this tool
	 * @return visible tool name
	 */
	const QString& getTitle() const { return m_title; }

	//! Get the icon for this tool type
	const QIcon getIcon(icon::Theme variant=icon::CURRENT) const { return icon::fromTheme(m_icon, variant); }

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

protected:
	virtual QWidget *createUiWidget(QWidget *parent) = 0;	
	ToolController *controller() { return m_ctrl; }

private:
	ToolController *m_ctrl;
	QString m_name;
	QString m_title;
	QString m_icon;
	QWidget *m_widget;
};

/**
 * \brief Add the available brush blending modes to a dropdown box
 *
 * This function also connects the combobox's change event to the brush preview
 * widget's setBlendingMode function.
 */
void populateBlendmodeBox(QComboBox *box, widgets::BrushPreview *preview);

}

#endif

