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

#include "resizedialog.h"
#include "ui_resizedialog.h"

namespace dialogs {


ResizeDialog::ResizeDialog(const QSize &oldsize, QWidget *parent) :
	QDialog(parent), _oldsize(oldsize), _aspectratio(0), _lastchanged(0)
{
	_ui = new Ui_ResizeDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Resize"));

	_ui->width->setValue(_oldsize.width());
	_ui->height->setValue(_oldsize.height());

	connect(_ui->width, SIGNAL(valueChanged(int)), this, SLOT(widthChanged(int)));
	connect(_ui->height, SIGNAL(valueChanged(int)), this, SLOT(heightChanged(int)));
	connect(_ui->keepaspect, SIGNAL(toggled(bool)), this, SLOT(toggleAspectRatio(bool)));
	connect(_ui->centerButton, SIGNAL(clicked()), this, SLOT(centerOffset()));

	connect(_ui->buttons->button(QDialogButtonBox::Reset), SIGNAL(clicked()), this, SLOT(reset()));
}

ResizeDialog::~ResizeDialog()
{
	delete _ui;
}

void ResizeDialog::widthChanged(int newWidth)
{
	if(_aspectratio) {
		_ui->height->blockSignals(true);
		_ui->height->setValue(newWidth / _aspectratio);
		_ui->height->blockSignals(false);
	}
	_lastchanged = 0;
}

void ResizeDialog::heightChanged(int newHeight)
{
	if(_aspectratio) {
		_ui->width->blockSignals(true);
		_ui->width->setValue(newHeight * _aspectratio);
		_ui->width->blockSignals(false);
	}
	_lastchanged = 1;
}

void ResizeDialog::toggleAspectRatio(bool keep)
{
	if(keep) {
		_aspectratio = _oldsize.width() / float(_oldsize.height());

		if(_lastchanged==0)
			widthChanged(_ui->width->value());
		else
			heightChanged(_ui->height->value());

	} else {
		_aspectratio = 0;
	}
}

void ResizeDialog::centerOffset()
{
	QSize newsize = newSize();
	_ui->xoffset->setValue((newsize.width() - _oldsize.width()) / 2);
	_ui->yoffset->setValue((newsize.height() - _oldsize.height()) / 2);
}

void ResizeDialog::reset()
{
	_ui->width->blockSignals(true);
	_ui->height->blockSignals(true);
	_ui->width->setValue(_oldsize.width());
	_ui->height->setValue(_oldsize.height());
	_ui->width->blockSignals(false);
	_ui->height->blockSignals(false);
	_ui->xoffset->setValue(0);
	_ui->yoffset->setValue(0);
}

QSize ResizeDialog::newSize() const
{
	return QSize(_ui->width->value(), _ui->height->value());
}

QPoint ResizeDialog::newOffset() const
{
	return QPoint(_ui->xoffset->value(), _ui->yoffset->value());
}

ResizeVector ResizeDialog::resizeVector() const
{
	ResizeVector rv;
	QSize s = newSize();
	QPoint o = newOffset();

	rv.top = o.y();
	rv.left = o.x();
	rv.right = s.width() - _oldsize.width() - o.x();
	rv.bottom = s.height() - _oldsize.height() - o.y();

	return rv;
}

}
