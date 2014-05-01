/*
   DrawPile - a collaborative drawing program.

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
#include <QPushButton>
#include "newdialog.h"
#include "widgets/colorbutton.h"
using widgets::ColorButton;

#include "ui_newdialog.h"

namespace dialogs {

NewDialog::NewDialog(QWidget *parent)
	: QDialog(parent)
{
	_ui = new Ui_NewDialog;
	_ui->setupUi(this);
	_ui->buttons->button(QDialogButtonBox::Ok)->setText(tr("Create new"));
	connect(this, SIGNAL(accepted()), this, SLOT(onAccept()));
}

NewDialog::~NewDialog()
{
	delete _ui;
}

void NewDialog::setSize(const QSize &size)
{
	_ui->width->setValue(size.width());
	_ui->height->setValue(size.height());

}

void NewDialog::setBackground(const QColor &color)
{
	_ui->background->setColor(color);
}

void NewDialog::onAccept()
{
	emit accepted(
		QSize(_ui->width->value(), _ui->height->value()),
		_ui->background->color()
	);
}

}

