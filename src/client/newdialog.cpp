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
#include <QPushButton>
#include "newdialog.h"
#include "colorbutton.h"
using widgets::ColorButton;

#include "ui_newdialog.h"

namespace dialogs {

NewDialog::NewDialog(QWidget *parent)
	: QDialog(parent)
{
	ui_ = new Ui_NewDialog;
	ui_->setupUi(this);
	ui_->buttons->button(QDialogButtonBox::Ok)->setText(tr("Create new"));
}

NewDialog::~NewDialog()
{
	delete ui_;
}

int NewDialog::newWidth() const
{
	return ui_->width->value();
}

//! Get the height for the new image
int NewDialog::newHeight() const
{
	return ui_->height->value();
}
//! Get the background color for the new image
QColor NewDialog::newBackground() const
{
	return ui_->background->color();
}

void NewDialog::setNewWidth(int w)
{
	ui_->width->setValue(w);
}

void NewDialog::setNewHeight(int h)
{
	ui_->height->setValue(h);
}

void NewDialog::setNewBackground(const QColor& color)
{
	ui_->background->setColor(color);
}

}

