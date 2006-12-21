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
#include "gradientslider.h"
using widgets::ColorTriangle;
using widgets::GradientSlider;

#include "ui_colordialog.h"

namespace dialogs {

ColorDialog::ColorDialog(QString title, QWidget *parent)
	: QDialog(parent, Qt::Tool), updating_(false)
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

ColorDialog::~ColorDialog()
{
	delete ui_;
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
	updateBars();
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
		updateBars();
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
		updateBars();
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

/**
 * Update the gradient bar colors
 */
void ColorDialog::updateBars()
{
	QColor col = color();
	int r,g,b;
	qreal h,s,v;
	col.getRgb(&r,&g,&b);
	col.getHsvF(&h,&s,&v);

	ui_->red->setColor1(QColor(0,g,b));
	ui_->red->setColor2(QColor(255,g,b));

	ui_->green->setColor1(QColor(r,0,b));
	ui_->green->setColor2(QColor(r,255,b));

	ui_->blue->setColor1(QColor(r,g,0));
	ui_->blue->setColor2(QColor(r,g,255));

	ui_->hue->setColorSaturation(s);
	ui_->hue->setColorValue(v);

	ui_->saturation->setColor1(QColor::fromHsvF(h,0,v));
	ui_->saturation->setColor2(QColor::fromHsvF(h,1,v));

	ui_->value->setColor1(QColor::fromHsvF(h,s,0));
	ui_->value->setColor2(QColor::fromHsvF(h,s,1));

}

}

