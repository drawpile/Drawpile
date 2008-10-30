/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2006-2008 Calle Laakkonen

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

#include "colordialog.h"
#include "colortriangle.h"
#include "gradientslider.h"
using widgets::ColorTriangle;
using widgets::GradientSlider;

#include "ui_colordialog.h"

namespace dialogs {

ColorDialog::ColorDialog(const QString& title, bool showapply, bool showalpha, QWidget *parent)
	: QDialog(parent/*, Qt::Tool*/), updating_(false), validhue_(0), showalpha_(showalpha)
{
	ui_ = new Ui_ColorDialog;
	ui_->setupUi(this);

	ui_->current->setAutoFillBackground(true);
	ui_->old->setAutoFillBackground(true);

	if(showapply==false)
		ui_->buttonBox->removeButton(ui_->buttonBox->button(QDialogButtonBox::Apply));
	else
		connect(ui_->buttonBox->button(QDialogButtonBox::Apply),
				SIGNAL(clicked()), this, SLOT(apply()));

	ui_->alphalbl->setVisible(showalpha_);
	ui_->alpha->setVisible(showalpha_);
	ui_->alphaBox->setVisible(showalpha_);

	connect(ui_->red, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(ui_->green, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));
	connect(ui_->blue, SIGNAL(valueChanged(int)), this, SLOT(updateRgb()));

	connect(ui_->hue, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(ui_->saturation, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));
	connect(ui_->value, SIGNAL(valueChanged(int)), this, SLOT(updateHsv()));

	connect(ui_->colorTriangle, SIGNAL(colorChanged(const QColor&)),
			this, SLOT(updateTriangle(const QColor&)));

	connect(ui_->buttonBox->button(QDialogButtonBox::Reset), SIGNAL(clicked()),
			this, SLOT(reset()));

	connect(ui_->txtHex, SIGNAL(textChanged(const QString&)),
			this, SLOT(updateHex()));

#if (QT_VERSION >= QT_VERSION_CHECK(4, 4,0)) // Not supported by QT<4.4?
	// Adjust the stretch factors (We can't set these in the UI designer at the moment)
	ui_->horizontalLayout->setStretchFactor(ui_->triangleLayout, 1);
	ui_->horizontalLayout->setStretchFactor(ui_->sliderLayout, 2);
#endif
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
	ui_->alpha->setValue(color.alpha());
	ui_->colorTriangle->setColor(color);
	ui_->txtHex->setText(color.name());
	if(h!=-1)
		validhue_ = h;
	updateBars();
	QPalette palette;
	palette.setColor(ui_->current->backgroundRole(), color);
	ui_->current->setPalette(palette);
	ui_->old->setPalette(palette);
	updating_ = false;
}

/*
 * @return current color
 */
QColor ColorDialog::color() const
{
	QColor c(ui_->red->value(), ui_->green->value(), ui_->blue->value());
	if(showalpha_)
		c.setAlpha(ui_->alpha->value());
	return c;
}

/**
 * Emit the selected color without closing the dialog
 */
void ColorDialog::apply()
{
	const QColor c = color();
	QPalette palette;
	palette.setColor(ui_->current->backgroundRole(), c);
	ui_->old->setPalette(palette);
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
	setColor(ui_->old->palette().color(ui_->old->backgroundRole()));
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
		if(h==-1)
			h = validhue_;
		else
			validhue_ = h;
		updating_ = true;
		ui_->hue->setValue(h);
		ui_->saturation->setValue(s);
		ui_->value->setValue(v);
		ui_->colorTriangle->setColor(col);
		updateBars();

		ui_->txtHex->setText(col.name());
		updateCurrent(col);
		updating_ = false;
	}
}

/**
 * HSV sliders have been used, update RGB to match
 */
void ColorDialog::updateHsv()
{
	if(!updating_) {
		const QColor col = QColor::fromHsv(
				ui_->hue->value(),
				ui_->saturation->value(),
				ui_->value->value()
				);
		updating_ = true;
		validhue_ = ui_->hue->value();
		ui_->red->setValue(col.red());
		ui_->green->setValue(col.green());
		ui_->blue->setValue(col.blue());
		ui_->colorTriangle->setColor(col);
		updateBars();
		updateCurrent(col);
		ui_->txtHex->setText(col.name());
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
		if(h==-1)
			h = validhue_;
		ui_->hue->setValue(h);
		ui_->saturation->setValue(s);
		ui_->value->setValue(v);
		updateCurrent(color);
		ui_->txtHex->setText(color.name());

		updating_ = false;
	}
}

/**
 * Hexadecimal color input box has changed, update
 * sliders and triangle.
 */
void ColorDialog::updateHex()
{
	if(!updating_) {
		updating_ = true;
		QColor color(ui_->txtHex->text());
		if(color.isValid()) {
			// Update RGB sliders
			ui_->red->setValue(color.red());
			ui_->green->setValue(color.green());
			ui_->blue->setValue(color.blue());

			// Update HSV sliders
			int h,s,v;
			color.getHsv(&h,&s,&v);
			if(h==-1)
				h = validhue_;
			ui_->hue->setValue(h);
			ui_->saturation->setValue(s);
			ui_->value->setValue(v);


			// Update everything else
			updateBars();
			ui_->colorTriangle->setColor(color);
			updateCurrent(color);
		}
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
	if(h<0)
		h = validhue_/360.0;

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

	if(showalpha_) {
		ui_->alpha->setColor1(QColor(r,g,b,0));
		ui_->alpha->setColor2(QColor(r,g,b,255));
	}
}

void ColorDialog::updateCurrent(const QColor& color)
{
	QPalette palette;
	palette.setColor(ui_->current->backgroundRole(), color);
	ui_->current->setPalette(palette);
}

}

