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
class Ui_LineSettings;

namespace tools {

//! Base class for tool settings
/**
 * The tool settings class provides a user interface widget that is
 * displayed in a dock window and a uniform way of getting a brush
 * configured by the user.
 */
class ToolSettings {
	public:
		ToolSettings(QString name,QString title)
			: name_(name), title_(title), widget_(0) {}
		virtual ~ToolSettings() { }

		//! Create an UI widget
		/**
		 * If the tool has a size changing signal, it will be connected to the
		 * parent's sizeChanged(int) signal.
		 * @param parent parent widget
		 * @return UI widget
		 */
		virtual QWidget *createUi(QWidget *parent) = 0;

		//! Get the UI widget
		QWidget *getUi() { return widget_; }

		//! Set the foreground color
		virtual void setForeground(const QColor& color) = 0;

		//! Set the background color
		virtual void setBackground(const QColor& color) = 0;

		//! Get a brush based on the settings in the UI
		/**
		 * An UI widget must have been created before this can be called.
		 * @return brush with values from the UI widget
		 */
		virtual const drawingboard::Brush& getBrush() const = 0;

		//! Get the brush size
		/**
		 * @return size of the current brush
		 */
		virtual int getSize() const = 0;

		//! Get the name (internal) of this tool
		/**
		 * The internal name is used when settings are stored to a
		 * configuration file
		 */
		const QString& getName() const { return name_; }

		//! Get the title of this tool
		/**
		 * The title is what is shown to the user
		 */
		const QString& getTitle() const { return title_; }

	protected:
		void setUiWidget(QWidget *widget) { widget_ = widget; }

	private:
		QString name_;
		QString title_;
		QWidget *widget_;
};

//! Basic brush settings
/**
 * This is a settings class for brush based drawing tools, like the
 * regular brush and eraser.
 */
class BrushSettings : public ToolSettings {
	public:
		BrushSettings(QString name, QString title, bool swapcolors=false);
		~BrushSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const drawingboard::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_BrushSettings *ui_;
		bool swapcolors_;
};

//! Line tool settings
/**
 */
class LineSettings : public ToolSettings {
	public:
		LineSettings(QString name, QString title);
		~LineSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const drawingboard::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_LineSettings *ui_;
};

//! No settings
/**
 * This is a dummy settings class for settingless tools, like the color picker
 */
class NoSettings : public ToolSettings {
	public:
		NoSettings(const QString& name, const QString& title);

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor&);
		void setBackground(const QColor&);
		const drawingboard::Brush& getBrush() const;

		int getSize() const { return 0; }
};

}

#endif

