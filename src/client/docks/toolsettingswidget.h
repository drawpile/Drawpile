/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef TOOLSETTINGSWIDGET_H
#define TOOLSETTINGSWIDGET_H

#include <QDockWidget>

#include "tools.h"

class QStackedWidget;

namespace tools {
	class ToolSettings;
	class AnnotationSettings;
	class ColorPickerSettings;
}

namespace dpcore {
	class Brush;
}

namespace widgets {

/**
 * @brief Tool settings window
 * A dock widget that displays settings for the currently selected tool.
 */
class ToolSettingsDock : public QDockWidget
{
	Q_OBJECT
	public:
		ToolSettingsDock(QWidget *parent=0);
		ToolSettingsDock(const ToolSettingsDock& ts) = delete;
		ToolSettingsDock& operator=(const ToolSettingsDock& ts) = delete;

		~ToolSettingsDock();

		//! Get a brush with the current settings
		const dpcore::Brush& getBrush(bool swapcolors) const;

		//! Get the annotation settings page
		tools::AnnotationSettings *getAnnotationSettings();

		//! Get the color picker page
		tools::ColorPickerSettings * getColorPickerSettings();

	signals:
		//! This signal is emitted when the current tool changes its size
		void sizeChanged(int size);

	public slots:
		//! Set the tool for which settings are shown
		void setTool(tools::Type tool);

		//! Set foreground color
		void setForeground(const QColor& color);

		//! Set background color
		void setBackground(const QColor& color);

	private:
		tools::ToolSettings *pensettings_;
		tools::ToolSettings *brushsettings_;
		tools::ToolSettings *erasersettings_;
		tools::ToolSettings *pickersettings_;
		tools::ToolSettings *linesettings_;
		tools::ToolSettings *rectsettings_;
		tools::ToolSettings *textsettings_;
		tools::ToolSettings *selectionsettings_;

		tools::ToolSettings *currenttool_;
		QStackedWidget *widgets_;
		QColor fgcolor_, bgcolor_;
};

}

#endif

