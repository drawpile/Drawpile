/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006 Calle Laakkonen

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
#include "interfaces.h"

class QStackedWidget;

namespace tools {
	class ToolSettings;
}

namespace widgets {

//! Tool settings window
/**
 * A dock widget that displays settings for the currently selected tool.
 */
class ToolSettings: public QDockWidget, public interface::BrushSource
{
	Q_OBJECT
	public:
		ToolSettings(QWidget *parent=0);

		~ToolSettings();

		//! Get a brush with the current settings
		const drawingboard::Brush& getBrush() const;

	signals:
		//! This signal is emitted when the current tool changes its size
		void sizeChanged(int size);

		//! This signal is emitted when the tool's colors are changed
		void colorsChanged(const QColor& fg, const QColor& bg);

	public slots:
		//! Set the tool for which settings are shown
		void setTool(tools::Type tool);

		//! Set foreground color
		void setForeground(const QColor& color);

		//! Set background color
		void setBackground(const QColor& color);

	private:
		ToolSettings(const ToolSettings& ts);
		ToolSettings& operator=(const ToolSettings& ts);

		tools::ToolSettings *brushsettings_;
		tools::ToolSettings *erasersettings_;
		tools::ToolSettings *pickersettings_;

		tools::ToolSettings *currenttool_;
		QStackedWidget *widgets_;
		QColor fgcolor_, bgcolor_;
};

}

#endif

