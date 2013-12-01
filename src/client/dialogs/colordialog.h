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
#ifndef COLORDIALOG_H
#define COLORDIALOG_H

#include <QDialog>

class Ui_ColorDialog;

namespace dialogs {

/**
 * @brief Color selection dialog
 * The color selection dialog provides sliders for chaning a color's
 * RGB and HSV values.
 */
class ColorDialog : public QDialog
{
	Q_OBJECT
	public:
		ColorDialog(const QString& title, bool showapply=true, bool showalpha=false, QWidget *parent=0);
		~ColorDialog();

		//! Set the color
		void setColor(const QColor& color);

		//! Get the current color
		QColor color() const;
	
	public slots:
		void accept();
		void apply();

	signals:
		//! This signal is emitted when Ok is pressed
		void colorSelected(const QColor& color);

	private slots:
		void reset();
		void updateRgb();
		void updateHsv();
		void updateTriangle(const QColor& color);
		void updateHex();

	private:
		void updateBars();
		void updateCurrent(const QColor& color);

		Ui_ColorDialog *ui_;
		bool updating_;
		int validhue_;
		bool showalpha_;
};

}

#endif

