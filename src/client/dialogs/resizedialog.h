/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2013 Calle Laakkonen

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
#ifndef RESIZEDIALOG_H
#define RESIZEDIALOG_H

#include <QDialog>

class Ui_ResizeDialog;

namespace dialogs {

struct ResizeVector {
	int top, right, bottom, left;

	bool isZero() const {
		return top==0 && right==0 && bottom==0 && left==0;
	}
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
	void reset();

private:
	Ui_ResizeDialog *_ui;

	QSize _oldsize;
	float _aspectratio;
	int _lastchanged;
};

}

#endif // RESIZEDIALOG_H
