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
#include "docks/inputsettingsdock.h"
#include "utils/kis_cubic_curve.h"
#include "ui_inputcfg.h"

namespace docks {

InputSettings::InputSettings(QWidget *parent) :
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
	_ui->usestylus->setChecked(cfg.value("usestylus", true).toBool());
	_ui->fakepressure->setCurrentIndex(cfg.value("pressuremode", 0).toInt());

	if(cfg.contains("pressurecurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("pressurecurve").toString());
		_ui->stylusCurve->setCurve(curve);
	}

	if(cfg.contains("distancecurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("distancecurve").toString());
		_ui->distanceCurve->setCurve(curve);
	}

	if(cfg.contains("velocitycurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("velocitycurve").toString());
		_ui->velocityCurve->setCurve(curve);
	} else {
		KisCubicCurve curve;
		curve.fromString("0,1;1,0");
		_ui->velocityCurve->setCurve(curve);
	}

	_ui->distance->setValue(cfg.value("distance", _ui->distance->minimum()).toInt());
	_ui->velocity->setValue(cfg.value("velocity", _ui->velocity->minimum()).toInt());
}

InputSettings::~InputSettings()
{
	// Save settings
	QSettings cfg;
	cfg.beginGroup("input");

	cfg.setValue("smoothing", _ui->smoothing->value());
	cfg.setValue("usestylus", _ui->usestylus->isChecked());
	cfg.setValue("pressuremode", _ui->fakepressure->currentIndex());
	cfg.setValue("pressurecurve", _ui->stylusCurve->curve().toString());
	cfg.setValue("distancecurve", _ui->distanceCurve->curve().toString());
	cfg.setValue("velocitycurve", _ui->velocityCurve->curve().toString());
	cfg.setValue("distance", _ui->distance->value());
	cfg.setValue("velocity", _ui->velocity->value());

	// Clean up
	delete _ui;
}

void InputSettings::connectCanvasView(widgets::CanvasView *view)
{
	_canvasview = view;
	view->setStrokeSmoothing(_ui->smoothing->value());
	view->setPressureCurve(_ui->stylusCurve->curve());
	view->setDistanceCurve(_ui->distanceCurve->curve());
	view->setVelocityCurve(_ui->velocityCurve->curve());
	view->setStylusPressureEnabled(_ui->usestylus->isChecked());
	updateFakePressureMode();

	connect(_ui->smoothing, SIGNAL(valueChanged(int)), view, SLOT(setStrokeSmoothing(int)));
	connect(_ui->usestylus, SIGNAL(toggled(bool)), view, SLOT(setStylusPressureEnabled(bool)));
	connect(_ui->stylusCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setPressureCurve(KisCubicCurve)));
	connect(_ui->distanceCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setDistanceCurve(KisCubicCurve)));
	connect(_ui->velocityCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setVelocityCurve(KisCubicCurve)));

	connect(_ui->fakepressure, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFakePressureMode()));
	connect(_ui->distance, SIGNAL(valueChanged(int)), this, SLOT(updateFakePressureMode()));
	connect(_ui->velocity, SIGNAL(valueChanged(int)), this, SLOT(updateFakePressureMode()));
}

void InputSettings::updateFakePressureMode()
{
	switch(_ui->fakepressure->currentIndex()) {
	case 0: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_NONE, 0); break;
	case 1: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_DISTANCE, _ui->distance->value()); break;
	case 2: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_VELOCITY, _ui->velocity->value()); break;
	}
}

}
