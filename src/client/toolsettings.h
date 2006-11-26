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
#ifndef TOOLSETTINGS_H
#define TOOLSETTINGS_H

#include "brush.h"

class Ui_BrushSettings;

namespace tools {

//! Base class for tool settings
class ToolSettings {
	public:
		virtual ~ToolSettings() { }

		//! Create an UI widget
		virtual QWidget *createUi(QWidget *parent) = 0;

		//! Get a brush based on the settings in the UI
		/**
		 * An UI widget must have been created before this can be called.
		 * @return brush with values from the UI widget
		 */
		virtual drawingboard::Brush getBrush() const = 0;
};

class BrushSettings : public ToolSettings {
	public:
		BrushSettings();
		~BrushSettings();

		QWidget *createUi(QWidget *parent);
		drawingboard::Brush getBrush() const;
	private:
		Ui_BrushSettings *ui_;
};

}

#endif

