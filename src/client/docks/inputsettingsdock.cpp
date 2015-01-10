/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#include <QSettings>

#include "scene/canvasview.h"
#include "docks/inputsettingsdock.h"
#include "docks/utils.h"
#include "utils/kis_cubic_curve.h"
#include "ui_inputcfg.h"

namespace docks {

namespace {
	int center(const QSlider *slider) { return (slider->maximum() - slider->minimum()) / 2 + slider->minimum(); }
}

InputSettings::InputSettings(QWidget *parent) :
	QDockWidget(tr("Input"), parent)
{
	_ui = new Ui_InputSettings;
	QWidget *w = new QWidget(this);
	setWidget(w);
	_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("input");
	_ui->smoothing->setValue(cfg.value("smoothing", center(_ui->smoothing)).toInt());
	_ui->pressuresrc->setCurrentIndex(cfg.value("pressuremode", 0).toInt());
	_ui->stackedWidget->setCurrentIndex(_ui->pressuresrc->currentIndex());

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

	KisCubicCurve defaultVelocityCurve;
	defaultVelocityCurve.fromString("0,1;1,0");
	_ui->velocityCurve->setDefaultCurve(defaultVelocityCurve);

	if(cfg.contains("velocitycurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("velocitycurve").toString());
		_ui->velocityCurve->setCurve(curve);
	} else {
		_ui->velocityCurve->setCurve(defaultVelocityCurve);
	}

	_ui->distance->setValue(cfg.value("distance", center(_ui->distance)).toInt());
	_ui->velocity->setValue(cfg.value("velocity", center(_ui->velocity)).toInt());
}

InputSettings::~InputSettings()
{
	// Save settings
	QSettings cfg;
	cfg.beginGroup("input");

	cfg.setValue("smoothing", _ui->smoothing->value());
	cfg.setValue("pressuremode", _ui->pressuresrc->currentIndex());
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
	updateFakePressureMode();

	connect(_ui->smoothing, SIGNAL(valueChanged(int)), view, SLOT(setStrokeSmoothing(int)));
	connect(_ui->stylusCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setPressureCurve(KisCubicCurve)));
	connect(_ui->distanceCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setDistanceCurve(KisCubicCurve)));
	connect(_ui->velocityCurve, SIGNAL(curveChanged(KisCubicCurve)), view, SLOT(setVelocityCurve(KisCubicCurve)));

	connect(_ui->pressuresrc, SIGNAL(currentIndexChanged(int)), this, SLOT(updateFakePressureMode()));
	connect(_ui->distance, SIGNAL(valueChanged(int)), this, SLOT(updateFakePressureMode()));
	connect(_ui->velocity, SIGNAL(valueChanged(int)), this, SLOT(updateFakePressureMode()));
}

void InputSettings::updateFakePressureMode()
{
	switch(_ui->pressuresrc->currentIndex()) {
	case 0: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_STYLUS, 0); break;
	case 1: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_DISTANCE, _ui->distance->value()); break;
	case 2: _canvasview->setPressureMode(widgets::CanvasView::PRESSUREMODE_VELOCITY, _ui->velocity->value()); break;
	}
}

}
