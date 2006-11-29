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

namespace widgets {
//! Color selection dialog
/**
 */
class ColorDialog : public QDialog
{
	Q_OBJECT
	public:
		ColorDialog(QString title,QWidget *parent=0);
		QColor color() const;
	signals:
		void colorChanged(const QColor& color);

	public slots:
		void setColor(const QColor& color);
	
	private slots:
		void updateColor();

	private:
		Ui_ColorDialog *ui_;
		bool updating_;
};

}

#endif

