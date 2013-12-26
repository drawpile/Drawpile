/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>

class Ui_ResizeDialog;

namespace dialogs {

struct ResizeVector {
	int top, right, bottom, left;
};

class ResizeDialog : public QDialog
{
	Q_OBJECT
public:
	explicit ResizeDialog(const QSize &oldsize, QWidget *parent = 0);
	~ResizeDialog();

	QSize newSize() const;
	QPoint newOffset() const;
	ResizeVector resizeVector() const;

signals:

public slots:

private slots:
	void widthChanged(int);
	void heightChanged(int);
	void toggleAspectRatio(bool keep);
	void centerOffset();

private:
	Ui_ResizeDialog *_ui;

	QSize _oldsize;
	float _aspectratio;
	int _lastchanged;
};

}

#endif // RESIZEDIALOG_H
