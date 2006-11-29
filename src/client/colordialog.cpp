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
#include "colordialog.h"
#include "ui_colordialog.h"

namespace widgets {

ColorDialog::ColorDialog(QString title, QWidget *parent)
	: QDialog(parent), updating_(false)
{
	ui_ = new Ui_ColorDialog;
	ui_->setupUi(this);
	connect(ui_->red, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->green, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->blue, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	setWindowTitle(title);
}

void ColorDialog::setColor(const QColor& color)
{
	updating_ = true;
	ui_->red->setValue(color.red());
	ui_->green->setValue(color.green());
	ui_->blue->setValue(color.blue());
	updating_ = false;
}

QColor ColorDialog::color() const
{
	return QColor(ui_->red->value(), ui_->green->value(), ui_->blue->value());
}

void ColorDialog::updateColor()
{
	if(!updating_)
		emit colorChanged(color());
}

}

