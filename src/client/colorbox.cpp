/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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

#include <QDebug>

#include "colorbox.h"

#include "colorbutton.h"
#include "gradientslider.h"
using widgets::ColorButton;
using widgets::GradientSlider;
#include "ui_colorbox.h"

namespace widgets {

ColorBox::ColorBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent), updating_(false)
{
	ui_ = new Ui_ColorBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);

	connect(ui_->red, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->green, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->blue, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
}

ColorBox::~ColorBox()
{
	delete ui_;
}

void ColorBox::setColor(const QColor& color)
{
	updating_ = true;
	ui_->red->setValue(color.red());
	ui_->green->setValue(color.green());
	ui_->blue->setValue(color.blue());
	updateSliders();
	updating_ = false;
}

void ColorBox::updateColor()
{
	if(updating_==false) {
		updateSliders();
		emit colorChanged(
				QColor(
					ui_->red->value(),
					ui_->green->value(),
					ui_->blue->value()
					)
				);
	}
}

void ColorBox::updateSliders()
{
	const int r = ui_->red->value();
	const int g = ui_->green->value();
	const int b = ui_->blue->value();

	ui_->red->setColor1(QColor(0,g,b));
	ui_->red->setColor2(QColor(255,g,b));

	ui_->green->setColor1(QColor(r,0,b));
	ui_->green->setColor2(QColor(r,255,b));

	ui_->blue->setColor1(QColor(r,g,0));
	ui_->blue->setColor2(QColor(r,g,255));
}

}

