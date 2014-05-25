/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2007-2014 Calle Laakkonen

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

#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QSpinBox>
#include <QSpacerItem>
#include <Hue_Slider>

namespace docks {

ColorBox::ColorBox(const QString& title, Mode mode, QWidget *parent)
	: QDockWidget(title, parent), _updating(false), _mode(mode)
{
	QWidget *w = new QWidget(this);
	w->setObjectName(QLatin1Literal("ColorBox"));
	w->resize(167, 95);
	setWidget(w);

	auto vbox = new QVBoxLayout(w);
	vbox->setSpacing(0);
	vbox->setContentsMargins(3, 3, 3, 3);

	auto grid = new QGridLayout;
	grid->setHorizontalSpacing(3);
	grid->setVerticalSpacing(0);
	grid->setContentsMargins(0, 0, 0, 0);

	// Red / Hue
	{
		auto label = new QLabel(_mode == HSV ? "H" : "R", w);
		grid->addWidget(label, 0, 0);

		if(_mode == HSV) {
			_s1 = new Hue_Slider(w);
			_s1->setMaximum(359);
		} else {
			_s1 = new Gradient_Slider(w);
			_s1->setMaximum(255);
		}
		grid->addWidget(_s1, 0, 1);

		_b1 = new QSpinBox(w);
		_b1->setMaximum(_s1->maximum());
		grid->addWidget(_b1, 0, 2);

		connect(_s1, SIGNAL(valueChanged(int)), this, SLOT(updateFromSliders()));
		connect(_b1, SIGNAL(valueChanged(int)), this, SLOT(updateFromSpinbox()));
	}

	// Green / Saturation
	{
		auto label = new QLabel(_mode == HSV ? "S" : "G", w);
		grid->addWidget(label, 1, 0);

		_s2 = new Gradient_Slider(w);
		_s2->setMaximum(255);
		grid->addWidget(_s2, 1, 1);

		_b2 = new QSpinBox(w);
		_b2->setMaximum(255);
		grid->addWidget(_b2, 1, 2);

		connect(_s2, SIGNAL(valueChanged(int)), this, SLOT(updateFromSliders()));
		connect(_b2, SIGNAL(valueChanged(int)), this, SLOT(updateFromSpinbox()));
	}

	// Blue / Value
	{
		auto label = new QLabel(_mode == HSV ? "V" : "B", w);
		grid->addWidget(label, 2, 0);

		_s3 = new Gradient_Slider(w);
		_s3->setMaximum(255);
		grid->addWidget(_s3, 2, 1);

		_b3 = new QSpinBox(w);
		_b3->setMaximum(255);
		grid->addWidget(_b3, 2, 2);

		connect(_s3, SIGNAL(valueChanged(int)), this, SLOT(updateFromSliders()));
		connect(_b3, SIGNAL(valueChanged(int)), this, SLOT(updateFromSpinbox()));
	}

	vbox->addLayout(grid);

	vbox->addSpacerItem(new QSpacerItem(5, 5, QSizePolicy::Minimum, QSizePolicy::Expanding));
}

void ColorBox::setColor(const QColor& color)
{
	_updating = true;
	if(_mode == RGB) {
		_s1->setFirstColor(QColor(0, color.green(), color.blue()));
		_s1->setLastColor(QColor(255, color.green(), color.blue()));
		_s1->setValue(color.red());
		_b1->setValue(color.red());

		_s2->setFirstColor(QColor(color.red(), 0, color.blue()));
		_s2->setLastColor(QColor(color.red(), 255, color.blue()));
		_s2->setValue(color.green());
		_b2->setValue(color.green());

		_s3->setFirstColor(QColor(color.red(), color.green(), 0));
		_s3->setLastColor(QColor(color.red(), color.green(), 255));
		_s3->setValue(color.blue());
		_b3->setValue(color.blue());

	} else {
		static_cast<Hue_Slider*>(_s1)->setColorSaturation(color.saturationF());
		static_cast<Hue_Slider*>(_s1)->setColorValue(color.valueF());
		_s1->setValue(color.hue());
		_b1->setValue(color.hue());

		_s2->setFirstColor(QColor::fromHsv(color.hue(), 0, color.value()));
		_s2->setLastColor(QColor::fromHsv(color.hue(), 255, color.value()));
		_s2->setValue(color.saturation());
		_b2->setValue(color.saturation());

		_s3->setFirstColor(QColor::fromHsv(color.hue(), color.saturation(), 0));
		_s3->setLastColor(QColor::fromHsv(color.hue(), color.saturation(), 255));
		_s3->setValue(color.value());
		_b3->setValue(color.value());
	}
	_updating = false;
}

void ColorBox::updateFromSliders()
{
	if(!_updating) {
		QColor color;
		if(_mode == RGB)
			color = QColor(_s1->value(), _s2->value(), _s3->value());
		else
			color = QColor::fromHsv(_s1->value(), _s2->value(), _s3->value());
				
		setColor(color);
		emit colorChanged(color);
	}
}

void ColorBox::updateFromSpinbox()
{
	if(!_updating) {
		QColor color;
		if(_mode == RGB)
			color = QColor(_b1->value(), _b2->value(), _b3->value());
		else
			color = QColor::fromHsv(_b1->value(), _b2->value(), _b3->value());

		setColor(color);
		emit colorChanged(color);
	}
}

}

