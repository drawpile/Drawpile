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
		ToolSettings(QString name,QString title)
			: name_(name), title_(title), widget_(0) {}
		virtual ~ToolSettings() { }

		//! Create an UI widget
		virtual QWidget *createUi(QWidget *parent) = 0;

		//! Get the UI widget
		QWidget *getUi() { return widget_; }

		//! Get a brush based on the settings in the UI
		/**
		 * An UI widget must have been created before this can be called.
		 * @param foreground foreground color
		 * @param background background color
		 * @return brush with values from the UI widget
		 */
		virtual drawingboard::Brush getBrush(const QColor& foreground,
				const QColor& background) const = 0;

		//! Get the name (internal) of this tool
		const QString& getName() const { return name_; }
		//
		//! Get the title of this tool
		const QString& getTitle() const { return title_; }

	protected:
		void setUiWidget(QWidget *widget) { widget_ = widget; }

	private:
		QString name_;
		QString title_;
		QWidget *widget_;
};

class BrushSettings : public ToolSettings {
	public:
		BrushSettings(QString name, QString title, bool swapcolors=false);
		~BrushSettings();

		QWidget *createUi(QWidget *parent);

		drawingboard::Brush getBrush(const QColor& foreground,
				const QColor& background) const;
	private:
		Ui_BrushSettings *ui_;
		bool swapcolors_;
};

}

#endif

