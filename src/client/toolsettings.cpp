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
#include "toolsettings.h"
#include "ui_brushsettings.h"

namespace tools {

BrushSettings::BrushSettings(QString name, QString title, bool swapcolors)
	: ToolSettings(name,title,swapcolors)
{
	ui_ = new Ui_BrushSettings();
}

BrushSettings::~BrushSettings()
{
	delete ui_;
}

void BrushSettings::createUi(QWidget *parent)
{
	QWidget *widget = new QWidget(parent);
	ui_->setupUi(widget);
	widget->hide();
	setUiWidget(widget);
}

drawingboard::Brush BrushSettings::getBrush(const QColor& foreground,
		const QColor& background) const
{
	int diameter = ui_->brushsize->value();
	qreal hardness = ui_->brushhardness->value()/qreal(ui_->brushhardness->maximum());
	qreal opacity = ui_->brushopacity->value()/qreal(ui_->brushopacity->maximum());
	int diameter2 = diameter;
	qreal hardness2 = hardness,opacity2 = opacity;
	if(ui_->pressuresize->isChecked())
		diameter2 = 1;
	if(ui_->pressurehardness->isChecked())
		hardness2 = 0;
	if(ui_->pressureopacity->isChecked())
		opacity2 = 0;

	drawingboard::Brush brush(diameter,hardness,opacity,
			isColorsSwapped()?background:foreground);
	brush.setDiameter2(diameter2);
	brush.setHardness2(hardness2);
	brush.setOpacity2(opacity2);
	brush.setColor2(isColorsSwapped()?foreground:background);
	return brush;
}

}

