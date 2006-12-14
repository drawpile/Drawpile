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
#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QDialog>

class Ui_ColorDialog;

namespace dialogs {
//! Color selection dialog
/**
 * The color selection dialog provides sliders for chaning a color's
 * RGB and HSV values.
 */
class ColorDialog : public QDialog
{
	Q_OBJECT
	public:
		ColorDialog(QString title,QWidget *parent=0);
		//! Get the current color
		QColor color() const;
	signals:
		//! This signal is emitted when the color is changed
		/**
		 * The signal is only emitted when the color is changed
		 * from within the dialog.
		 * @param color new color
		 */
		void colorChanged(const QColor& color);

	public slots:
		//! Set the color
		void setColor(const QColor& color);
	
	private slots:
		void updateRgb();
		void updateHsv();
		void updateTriangle(const QColor& color);

	private:
		void updateBars();
		Ui_ColorDialog *ui_;
		bool updating_;
};

}

#endif

