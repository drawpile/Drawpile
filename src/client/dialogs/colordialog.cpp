/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2014 Calle Laakkonen

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

#include "colordialog.h"
#include "widgets/qtcolortriangle.h"
#include "widgets/gradientslider.h"
using widgets::GradientSlider;

#include "ui_colordialog.h"

namespace dialogs {

ColorDialog::ColorDialog(QWidget *parent, const QString& title, Flags flags)
	: QDialog(parent), _validhue(0), _showalpha(flags.testFlag(SHOW_ALPHA)), _updating(false)
{
	_ui = new Ui_ColorDialog;
	_ui->setupUi(this);

	_ui->current->setAutoFillBackground(true);
	_ui->old->setAutoFillBackground(true);

	if(flags.testFlag(NO_APPLY))
		_ui->buttonBox->removeButton(_ui->buttonBox->button(QDialogButtonBox::Apply));
	else
		connect(_ui->buttonBox->button(QDialogButtonBox::Apply),
				SIGNAL(clicked()), this, SLOT(apply()));

	_ui->alphalbl->setVisible(_showalpha);
	_ui->alpha->setVisible(_showalpha);
	_ui->alphaBox->setVisible(_showalpha);

	connect(_ui->red, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(_ui->green, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(_ui->blue, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));

	connect(_ui->hue, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(_ui->saturation, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(_ui->value, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));

	connect(_ui->colorTriangle, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(updateTriangle(const QColor&)));

	connect(_ui->buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()),
			this, SLOT(reset()));

	connect(_ui->txtHex, SIGNAL(textChanged(const QString&)),
			this, SLOT(updateHex()));

	setWindowTitle(title);
}

ColorDialog::~ColorDialog()
{
	delete _ui;
}

void ColorDialog::pickNewColor(const QColor &oldcolor)
{
	setColor(oldcolor);
	show();
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

	_updating = true;
	_ui->red->setValue(color.red());
	_ui->green->setValue(color.green());
	_ui->blue->setValue(color.blue());
	_ui->hue->setValue(h);
	_ui->saturation->setValue(s);
	_ui->value->setValue(v);
	_ui->alpha->setValue(color.alpha());
	_ui->colorTriangle->setColor(color);
	_ui->txtHex->setText(color.name());
	if(h!=-1)
		_validhue = h;
	updateBars();
	QPalette palette;
	palette.setColor(_ui->current->backgroundRole(), color);
	_ui->current->setPalette(palette);
	_ui->old->setPalette(palette);
	_updating = false;
}

/*
 * @return current color
 */
QColor ColorDialog::color() const
{
	QColor c(_ui->red->value(), _ui->green->value(), _ui->blue->value());
	if(_showalpha)
		c.setAlpha(_ui->alpha->value());
	return c;
}

/**
 * Emit the selected color without closing the dialog
 */
void ColorDialog::apply()
{
	const QColor c = color();
	QPalette palette;
	palette.setColor(_ui->current->backgroundRole(), c);
	_ui->old->setPalette(palette);
	emit colorSelected(c);
}

/**
 * Emit colorSelected before closing the dialog
 */
void ColorDialog::accept()
{
	emit colorSelected(color());
	QDialog::accept();
}

/**
 * Reset to the old color
 */
void ColorDialog::reset()
{
	setColor(_ui->old->palette().color(_ui->old->backgroundRole()));
}

/**
 * RGB sliders have been used, update HSV to match
 */
void ColorDialog::updateRgb()
{
	if(!_updating) {
		QColor col = color();
		int h,s,v;
		col.getHsv(&h,&s,&v);
		if(h==-1)
			h = _validhue;
		else
			_validhue = h;
		_updating = true;
		_ui->hue->setValue(h);
		_ui->saturation->setValue(s);
		_ui->value->setValue(v);
		_ui->colorTriangle->setColor(col);
		updateBars();

		_ui->txtHex->setText(col.name());
		updateCurrent(col);
		_updating = false;
	}
}

/**
 * HSV sliders have been used, update RGB to match
 */
void ColorDialog::updateHsv()
{
	if(!_updating) {
		const QColor col = QColor::fromHsv(
				_ui->hue->value(),
				_ui->saturation->value(),
				_ui->value->value()
				);
		_updating = true;
		_validhue = _ui->hue->value();
		_ui->red->setValue(col.red());
		_ui->green->setValue(col.green());
		_ui->blue->setValue(col.blue());
		_ui->colorTriangle->setColor(col);
		updateBars();
		updateCurrent(col);
		_ui->txtHex->setText(col.name());
		_updating = false;
	}
}

/**
 * Color triangle has been used, update sliders to match
 */
void ColorDialog::updateTriangle(const QColor& color)
{
	if(!_updating) {
		_updating = true;
		_ui->red->setValue(color.red());
		_ui->green->setValue(color.green());
		_ui->blue->setValue(color.blue());

		int h,s,v;
		color.getHsv(&h,&s,&v);
		if(h==-1)
			h = _validhue;
		_ui->hue->setValue(h);
		_ui->hue->setColorSaturation(color.saturationF());
		_ui->hue->setColorValue(color.valueF());
		_ui->saturation->setValue(s);
		_ui->value->setValue(v);
		updateCurrent(color);
		_ui->txtHex->setText(color.name());

		_updating = false;
	}
}

/**
 * Hexadecimal color input box has changed, update
 * sliders and triangle.
 */
void ColorDialog::updateHex()
{
	if(!_updating) {
		_updating = true;
		QColor color(_ui->txtHex->text());
		if(color.isValid()) {
			// Update RGB sliders
			_ui->red->setValue(color.red());
			_ui->green->setValue(color.green());
			_ui->blue->setValue(color.blue());

			// Update HSV sliders
			int h,s,v;
			color.getHsv(&h,&s,&v);
			if(h==-1)
				h = _validhue;
			_ui->hue->setValue(h);
			_ui->saturation->setValue(s);
			_ui->value->setValue(v);


			// Update everything else
			updateBars();
			_ui->colorTriangle->setColor(color);
			updateCurrent(color);
		}
		_updating = false;
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
	if(h<0)
		h = _validhue/360.0;

	_ui->red->setColor1(QColor(0,g,b));
	_ui->red->setColor2(QColor(255,g,b));

	_ui->green->setColor1(QColor(r,0,b));
	_ui->green->setColor2(QColor(r,255,b));

	_ui->blue->setColor1(QColor(r,g,0));
	_ui->blue->setColor2(QColor(r,g,255));

	_ui->hue->setColorSaturation(s);
	_ui->hue->setColorValue(v);

	_ui->saturation->setColor1(QColor::fromHsvF(h,0,v));
	_ui->saturation->setColor2(QColor::fromHsvF(h,1,v));

	_ui->value->setColor1(QColor::fromHsvF(h,s,0));
	_ui->value->setColor2(QColor::fromHsvF(h,s,1));

	if(_showalpha) {
		_ui->alpha->setColor1(QColor(r,g,b,0));
		_ui->alpha->setColor2(QColor(r,g,b,255));
	}
}

void ColorDialog::updateCurrent(const QColor& color)
{
	QPalette palette;
	palette.setColor(_ui->current->backgroundRole(), color);
	_ui->current->setPalette(palette);
}

}
