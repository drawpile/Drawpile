/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007 Calle Laakkonen

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

#include "colorbox.h"

#include "widgets/colorbutton.h"
#include "widgets/gradientslider.h"
using widgets::ColorButton;
using widgets::GradientSlider;
#include "ui_colorbox.h"

namespace docks {

ColorBox::ColorBox(const QString& title, Mode mode, QWidget *parent)
	: QDockWidget(title, parent), updating_(false), mode_(mode)
{
	ui_ = new Ui_ColorBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);

	if(mode_ == HSV) {
		ui_->c1label->setText("H");
		ui_->c2label->setText("S");
		ui_->c3label->setText("V");
		ui_->c1->setMode(GradientSlider::Hsv);
		ui_->c1box->setMaximum(359);
		ui_->c1->setMaximum(359);
	}

	connect(ui_->c1, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->c2, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
	connect(ui_->c3, SIGNAL(valueChanged(int)), this, SLOT(updateColor()));
}

ColorBox::~ColorBox()
{
	delete ui_;
}

void ColorBox::setColor(const QColor& color)
{
	updating_ = true;
	if(mode_ == RGB) {
		ui_->c1->setValue(color.red());
		ui_->c2->setValue(color.green());
		ui_->c3->setValue(color.blue());
	} else {
		ui_->c1->setValue(color.hue());
		ui_->c2->setValue(color.saturation());
		ui_->c3->setValue(color.value());
	}
	updateSliders();
	updating_ = false;
}

void ColorBox::updateColor()
{
	if(updating_==false) {
		updateSliders();
		QColor color;
		if(mode_ == RGB)
			color = QColor(
					ui_->c1->value(),
					ui_->c2->value(),
					ui_->c3->value()
					);
		else
			color = QColor::fromHsv(
					ui_->c1->value(),
					ui_->c2->value(),
					ui_->c3->value()
					);
				
		emit colorChanged(color);
	}
}

void ColorBox::updateSliders()
{
	const int c1 = ui_->c1->value();
	const int c2 = ui_->c2->value();
	const int c3 = ui_->c3->value();

	if(mode_ == RGB) {
		ui_->c1->setColor1(QColor(0,c2,c3));
		ui_->c1->setColor2(QColor(255,c2,c3));

		ui_->c2->setColor1(QColor(c1,0,c3));
		ui_->c2->setColor2(QColor(c1,255,c3));

		ui_->c3->setColor1(QColor(c1,c2,0));
		ui_->c3->setColor2(QColor(c1,c2,255));
	} else {
		ui_->c1->setColorSaturation(c2/255.0);
		ui_->c1->setColorValue(c3/255.0);

		ui_->c2->setColor1(QColor::fromHsv(c1,0,c3));
		ui_->c2->setColor2(QColor::fromHsv(c1,255,c3));

		ui_->c3->setColor1(QColor::fromHsv(c1,c2,0));
		ui_->c3->setColor2(QColor::fromHsv(c1,c2,255));
	}
}

}

