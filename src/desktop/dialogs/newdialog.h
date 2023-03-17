/*
   Drawpile - a collaborative drawing program.

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
#ifndef NEWDIALOG_H
#define NEWDIALOG_H

#include <QDialog>

class Ui_NewDialog;

namespace dialogs {

/**
 * @brief Dialog to set new drawing settings
 * The "new drawing" dialog allows the user to set the width, height
 * and background color of a new image.
 */
class NewDialog final : public QDialog
{
Q_OBJECT
public:
	NewDialog(QWidget *parent=nullptr);
	~NewDialog() override;

	//! Set the width/height fields
	void setSize(const QSize &size);

	//! Set the background color field
	void setBackground(const QColor &color);

public slots:
	void done(int r);

signals:
	void accepted(const QSize &size, const QColor &background);

private:
	Ui_NewDialog *_ui;
};

}

#endif
