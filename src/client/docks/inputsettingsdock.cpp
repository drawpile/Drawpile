/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2015 Calle Laakkonen

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

#include "docks/inputsettingsdock.h"
#include "docks/utils.h"

#include "ui_inputcfg.h"

#include <QSettings>

namespace docks {

namespace {
	int center(const QSlider *slider) { return (slider->maximum() - slider->minimum()) / 2 + slider->minimum(); }
}

InputSettings::InputSettings(QWidget *parent) :
	QDockWidget(tr("Input"), parent)
{
	m_ui = new Ui_InputSettings;
	QWidget *w = new QWidget(this);
	setWidget(w);
	m_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	// Restore settings
	QSettings cfg;
	cfg.beginGroup("input");
	m_ui->smoothing->setValue(cfg.value("smoothing", center(m_ui->smoothing)).toInt());
	m_ui->pressuresrc->setCurrentIndex(cfg.value("pressuremode", 0).toInt());
	m_ui->stackedWidget->setCurrentIndex(m_ui->pressuresrc->currentIndex());

	if(cfg.contains("pressurecurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("pressurecurve").toString());
		m_ui->stylusCurve->setCurve(curve);
	}

	if(cfg.contains("distancecurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("distancecurve").toString());
		m_ui->distanceCurve->setCurve(curve);
	}

	KisCubicCurve defaultVelocityCurve;
	defaultVelocityCurve.fromString("0,1;1,0");
	m_ui->velocityCurve->setDefaultCurve(defaultVelocityCurve);

	if(cfg.contains("velocitycurve")) {
		KisCubicCurve curve;
		curve.fromString(cfg.value("velocitycurve").toString());
		m_ui->velocityCurve->setCurve(curve);
	} else {
		m_ui->velocityCurve->setCurve(defaultVelocityCurve);
	}

	m_ui->distance->setValue(cfg.value("distance", center(m_ui->distance)).toInt());
	m_ui->velocity->setValue(cfg.value("velocity", center(m_ui->velocity)).toInt());

	// Make connections
	connect(m_ui->smoothing, SIGNAL(valueChanged(int)), this, SIGNAL(smoothingChanged(int)));

	connect(m_ui->stylusCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);
	connect(m_ui->distanceCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);
	connect(m_ui->velocityCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);

	connect(m_ui->pressuresrc, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePressureMapping()));
	connect(m_ui->distance, SIGNAL(valueChanged(int)), this, SLOT(updatePressureMapping()));
	connect(m_ui->velocity, SIGNAL(valueChanged(int)), this, SLOT(updatePressureMapping()));
}

InputSettings::~InputSettings()
{
	// Save settings
	QSettings cfg;
	cfg.beginGroup("input");

	cfg.setValue("smoothing", m_ui->smoothing->value());
	cfg.setValue("pressuremode", m_ui->pressuresrc->currentIndex());
	cfg.setValue("pressurecurve", m_ui->stylusCurve->curve().toString());
	cfg.setValue("distancecurve", m_ui->distanceCurve->curve().toString());
	cfg.setValue("velocitycurve", m_ui->velocityCurve->curve().toString());
	cfg.setValue("distance", m_ui->distance->value());
	cfg.setValue("velocity", m_ui->velocity->value());

	// Clean up
	delete m_ui;
}

int InputSettings::getSmoothing() const
{
	return m_ui->smoothing->value();
}

PressureMapping InputSettings::getPressureMapping() const
{
	PressureMapping pm;

	switch(m_ui->pressuresrc->currentIndex()) {
	case 0:
		pm.mode = PressureMapping::STYLUS;
		pm.curve = m_ui->stylusCurve->curve();
		pm.param = 0;
		break;
	case 1:
		pm.mode = PressureMapping::DISTANCE;
		pm.curve = m_ui->distanceCurve->curve();
		pm.param = m_ui->distance->value();
		break;
	case 2:
		pm.mode = PressureMapping::VELOCITY;
		pm.curve = m_ui->velocityCurve->curve();
		pm.param = m_ui->velocity->value();
		break;
	}

	return pm;
}

void InputSettings::updatePressureMapping()
{
	emit pressureMappingChanged(getPressureMapping());
}

}
