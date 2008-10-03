/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include "core/brush.h"

class Ui_PenSettings;
class Ui_BrushSettings;
class Ui_EraserSettings;
class Ui_SimpleSettings;
class Ui_TextSettings;
class QSettings;

namespace drawingboard {
	class BoardEditor;
	class AnnotationItem;
}

namespace tools {

//! Abstract base class for tool settings
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
		virtual const dpcore::Brush& getBrush() const = 0;

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

		//! Get a settings object prepared for this tool
		QSettings &getSettings();

	private:
		QString name_;
		QString title_;
		QWidget *widget_;
};

//! Pen settings
/**
 * This is much like BrushSettings, except the pen always has 100% hardness
 * and no antialiasing.
 */
class PenSettings : public ToolSettings {
	public:
		PenSettings(QString name, QString title);
		~PenSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const dpcore::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_PenSettings *ui_;
};

//! Eraser settings
/**
 * This is a settings class for brushes that erase. Currently an eraser
 * works just like a regular brush, except it paints with the background
 * color. The eraser will work differently when layers are implemented.
 */
class EraserSettings : public ToolSettings {
	public:
		EraserSettings(QString name, QString title);
		~EraserSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const dpcore::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_EraserSettings *ui_;
};

//! Basic brush settings
/**
 * This is a settings class for brush based drawing tools, like the
 * regular brush and eraser.
 */
class BrushSettings : public ToolSettings {
	public:
		BrushSettings(QString name, QString title);
		~BrushSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const dpcore::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_BrushSettings *ui_;
};

//! Settings for tools without pressure sensitivity
/**
 */
class SimpleSettings : public ToolSettings {
	public:
		enum Type {Line, Rectangle};

		SimpleSettings(QString name, QString title, Type type, bool sp);
		~SimpleSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const dpcore::Brush& getBrush() const;

		int getSize() const;

	private:
		Ui_SimpleSettings *ui_;
		Type type_;
		bool subpixel_;
};

//! Settings for the annotation tool
class AnnotationSettings : public QObject, public ToolSettings {
	Q_OBJECT
	public:
		AnnotationSettings(QString name, QString title);
		~AnnotationSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor& color);
		void setBackground(const QColor& color);
		const dpcore::Brush& getBrush() const;

		int getSize() const { return 0; }

		//! Set the board editor to change selected annotations
		void setBoardEditor(drawingboard::BoardEditor *editor) { editor_ = editor; }

		//! Enable or disabled baking
		void enableBaking(bool enable);

	public slots:
		//! Set the currently selected annotation item
		void setSelection(drawingboard::AnnotationItem *item);
		//! Unselect this item if currently selected
		void unselect(drawingboard::AnnotationItem *item);

	private slots:
		void applyChanges();
		void removeAnnotation();
		void bake();

	signals:
		void baked();

	private:
		dpcore::Brush brush_;
		Ui_TextSettings *ui_;
		QWidget *uiwidget_;
		drawingboard::AnnotationItem *sel_;
		drawingboard::BoardEditor *editor_;
		bool noupdate_;
};

//! No settings
/**
 * This is a dummy settings class for settingless tools, like the color picker
 */
class NoSettings : public ToolSettings {
	public:
		NoSettings(const QString& name, const QString& title);
		~NoSettings();

		QWidget *createUi(QWidget *parent);

		void setForeground(const QColor&);
		void setBackground(const QColor&);
		const dpcore::Brush& getBrush() const;

		int getSize() const { return 0; }

	private:
		Ui_TextSettings *ui_;
};

}

#endif

