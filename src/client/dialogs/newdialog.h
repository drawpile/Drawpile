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
#ifndef NEWDIALOG_H
#define NEWDIALOG_H

#include <QDialog>

class Ui_NewDialog;

namespace dialogs {
//! Dialog to set new drawing settings
/**
 * The "new drawing" dialog allows the user to set the width, height
 * and background color of a new image.
 */
class NewDialog : public QDialog
{
	Q_OBJECT
	public:
		NewDialog(QWidget *parent=0);
		~NewDialog();

		//! Get the width for the new image
		int newWidth() const;
		//! Get the height for the new image
		int newHeight() const;
		//! Get the background color for the new image
		QColor newBackground() const;

		//! Set new width
		void setNewWidth(int w);

		//! Set new height
		void setNewHeight(int h);

		//! Set new background color
		void setNewBackground(const QColor& color);
	private:
		Ui_NewDialog *ui_;
};

}

#endif

