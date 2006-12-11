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
#include "colortriangle.h"
using widgets::ColorTriangle;

#include "ui_colordialog.h"

namespace widgets {

ColorDialog::ColorDialog(QString title, QWidget *parent)
	: QDialog(parent), updating_(false)
{
	ui_ = new Ui_ColorDialog;
	ui_->setupUi(this);
	connect(ui_->red, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(ui_->green, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(ui_->blue, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));

	connect(ui_->hue, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(ui_->saturation, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(ui_->value, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));

	connect(ui_->colorTriangle, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(updateTriangle(const QColor&)));
	setWindowTitle(title);
}

/**
 * The contents of the widget is updated to reflect the new color.
 * No signal is emitted.
 * @param color new color
 */
void ColorDialog::setColor(const QColor& color)
{
	int h,s,v;
	color.getHsv(&h,&s,&v);

	updating_ = true;
	ui_->red->setValue(color.red());
	ui_->green->setValue(color.green());
	ui_->blue->setValue(color.blue());
	ui_->hue->setValue(h);
	ui_->saturation->setValue(s);
	ui_->value->setValue(v);
	ui_->colorTriangle->setColor(color);
	updating_ = false;
}

/*
 * @return current color
 */
QColor ColorDialog::color() const
{
	return QColor(ui_->red->value(), ui_->green->value(), ui_->blue->value());
}

/**
 * RGB sliders have been used, update HSV to match
 */
void ColorDialog::updateRgb()
{
	if(!updating_) {
		QColor col = color();
		int h,s,v;
		col.getHsv(&h,&s,&v);
		updating_ = true;
		ui_->hue->setValue(h);
		ui_->saturation->setValue(s);
		ui_->value->setValue(v);
		ui_->colorTriangle->setColor(col);
		emit colorChanged(col);
		updating_ = false;
	}
}

/**
 * HSV sliders have been used, update RGB to match
 */
void ColorDialog::updateHsv()
{
	if(!updating_) {
		QColor col = QColor::fromHsv(
				ui_->hue->value(),
				ui_->saturation->value(),
				ui_->value->value()
				);
		updating_ = true;
		ui_->red->setValue(col.red());
		ui_->green->setValue(col.green());
		ui_->blue->setValue(col.blue());
		ui_->colorTriangle->setColor(col);
		emit colorChanged(col);
		updating_ = false;
	}
}

/**
 * Color triangle has been used, update sliders to match
 */
void ColorDialog::updateTriangle(const QColor& color)
{
	if(!updating_) {
		updating_ = true;
		ui_->red->setValue(color.red());
		ui_->green->setValue(color.green());
		ui_->blue->setValue(color.blue());

		int h,s,v;
		color.getHsv(&h,&s,&v);
		ui_->hue->setValue(h);
		ui_->saturation->setValue(s);
		ui_->value->setValue(v);
		emit colorChanged(color);

		updating_ = false;
	}
}

}

