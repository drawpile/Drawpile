/*
   DrawPile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QSettings>

#include "canvasview.h"
#include "inputsettingsdock.h"
#include "ui_inputcfg.h"

namespace widgets {

InputSettingsDock::InputSettingsDock(QWidget *parent) :
	QDockWidget(tr("Input"), parent)
{
	_ui = new Ui_InputSettings;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("input");
	_ui->smoothing->setValue(cfg.value("smoothing", 0).toInt());
}

InputSettingsDock::~InputSettingsDock()
{
	// Save settings
	QSettings cfg;
	cfg.beginGroup("input");

	cfg.setValue("smoothing", _ui->smoothing->value());

	// Clean up
	delete _ui;
}

void InputSettingsDock::connectCanvasView(CanvasView *view)
{
	view->setStrokeSmoothing(_ui->smoothing->value());
	connect(_ui->smoothing, SIGNAL(valueChanged(int)), view, SLOT(setStrokeSmoothing(int)));
}

}
