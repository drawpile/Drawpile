/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2013 Calle Laakkonen

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
#ifndef TOOLSETTINGSDOCK_H
#define TOOLSETTINGSDOCK_H

#include <QDockWidget>

#include "tools/tool.h"

class QStackedWidget;

namespace tools {
	class ToolSettings;
	class AnnotationSettings;
	class ColorPickerSettings;
	class LaserPointerSettings;
}

namespace paintcore {
	class Brush;
}

namespace widgets {
	class DualColorButton;
}

namespace docks {

/**
 * @brief Tool settings window
 * A dock widget that displays settings for the currently selected tool.
 */
class ToolSettings : public QDockWidget
{
	Q_OBJECT
	public:
		ToolSettings(QWidget *parent=0);
		ToolSettings(const ToolSettings& ts) = delete;
		ToolSettings& operator=(const ToolSettings& ts) = delete;

		~ToolSettings();

		//! Get a brush with the current settings
		const paintcore::Brush& getBrush(bool swapcolors) const;

		//! Get the annotation settings page
		tools::AnnotationSettings *getAnnotationSettings() { return _textsettings; }

		//! Get the color picker page
		tools::ColorPickerSettings *getColorPickerSettings() { return _pickersettings; }

		//! Get the laser pointer settings page
		tools::LaserPointerSettings  *getLaserPointerSettings() { return _lasersettings; }

		//! Quick adjust current tool
		void quickAdjustCurrent1(float adjustment);

		//! Get the current foreground color
		QColor foregroundColor() const;

		//! Get the current background color
		QColor backgroundColor() const;

	signals:
		//! This signal is emitted when the current tool changes its size
		void sizeChanged(int size);

		//! Current foreground color selection changed
		void foregroundColorChanged(const QColor &color);

		//! Current background color selection changed
		void backgroundColorChanged(const QColor &color);

	public slots:
		//! Set the tool for which settings are shown
		void setTool(tools::Type tool);

		//! Set foreground color
		void setForegroundColor(const QColor& color);

		//! Set background color
		void setBackgroundColor(const QColor& color);

		//! Swap current foreground and background colors
		void swapForegroundBackground();

	private:
		tools::ToolSettings *_pensettings;
		tools::ToolSettings *_brushsettings;
		tools::ToolSettings *_erasersettings;
		tools::ColorPickerSettings *_pickersettings;
		tools::ToolSettings *_linesettings;
		tools::ToolSettings *_rectsettings;
		tools::ToolSettings *_ellipsesettings;
		tools::AnnotationSettings *_textsettings;
		tools::ToolSettings *_selectionsettings;
		tools::LaserPointerSettings  *_lasersettings;

		tools::ToolSettings *_currenttool;
		QStackedWidget *_widgets;
		widgets::DualColorButton *_fgbgcolor;
};

}

#endif

