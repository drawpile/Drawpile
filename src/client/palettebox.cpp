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

#include "palettebox.h"

#include "palettewidget.h"
using widgets::PaletteWidget;
#include "ui_palettebox.h"

#include "localpalette.h"

namespace widgets {

PaletteBox::PaletteBox(const QString& title, QWidget *parent)
	: QDockWidget(title, parent)
{
	ui_ = new Ui_PaletteBox;
	QWidget *w = new QWidget(this);
	setWidget(w);
	ui_->setupUi(w);

	LocalPalette *pal = new LocalPalette;
	for(int i=0;i<256;++i)
		pal->insertColor(0, QColor(i,i,i));
	ui_->palette->setPalette(pal);

	connect(ui_->palette, SIGNAL(colorSelected(QColor)),
			this, SIGNAL(colorSelected(QColor)));
}

PaletteBox::~PaletteBox()
{
	delete ui_;
}

}

