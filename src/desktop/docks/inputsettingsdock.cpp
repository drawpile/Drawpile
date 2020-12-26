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

#include <QDebug>
#include <QInputDialog>
#include <QUuid>

namespace docks {

InputSettings::InputSettings(input::PresetModel *presetModel, QWidget *parent) :
	QDockWidget(tr("Input"), parent),
	m_presetModel(presetModel),
	m_updateInProgress(false)
{
	m_ui = new Ui_InputSettings;
	QWidget *w = new QWidget(this);
	setWidget(w);
	m_ui->setupUi(w);

	setStyleSheet(defaultDockStylesheet());

	m_presetmenu = new QMenu(this);
	m_addPresetAction = m_presetmenu->addAction(tr("Add preset"), this, &InputSettings::addPreset);
	m_renamePresetAction = m_presetmenu->addAction(tr("Rename preset"), this, &InputSettings::renamePreset);
	m_removePresetAction = m_presetmenu->addAction(tr("Remove preset"), this, &InputSettings::removePreset);
	m_ui->presetButton->setMenu(m_presetmenu);

	m_ui->preset->setModel(m_presetModel);
	m_ui->preset->setCurrentIndex(0);
	choosePreset(m_ui->preset->currentIndex());

	// Make connections
	connect(m_ui->smoothing, &QSlider::valueChanged, this, &InputSettings::updateSmoothing);

	connect(m_ui->stylusCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);
	connect(m_ui->distanceCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);
	connect(m_ui->velocityCurve, &KisCurveWidget::curveChanged, this, &InputSettings::updatePressureMapping);

	connect(m_ui->pressuresrc, SIGNAL(currentIndexChanged(int)), this, SLOT(updatePressureMapping()));
	connect(m_ui->distance, SIGNAL(valueChanged(int)), this, SLOT(updatePressureMapping()));
	connect(m_ui->velocity, SIGNAL(valueChanged(int)), this, SLOT(updatePressureMapping()));

	connect(m_ui->preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputSettings::choosePreset);
	connect(m_presetModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(presetDataChanged(QModelIndex,QModelIndex)));
}

InputSettings::~InputSettings()
{
	delete m_ui;
}


input::Preset *InputSettings::mutableCurrentPreset()
{
	return (*m_presetModel)[m_ui->preset->currentIndex()];
}

const input::Preset *InputSettings::currentPreset() const
{
	return m_presetModel->at(m_ui->preset->currentIndex());
}

void InputSettings::updateSmoothing()
{
	input::Preset *preset = mutableCurrentPreset();
	if(preset) {
		applyUiToPreset(*preset);
	}
}

void InputSettings::updatePressureMapping()
{
	input::Preset *preset = mutableCurrentPreset();
	if(preset) {
		applyUiToPreset(*preset);
	}
}

void InputSettings::choosePreset(int index)
{
	const input::Preset *preset = m_presetModel->at(index);
	if(preset) {
		applyPresetToUi(*preset);
		updatePresetMenu();
	}
}

void InputSettings::addPreset()
{
	const input::Preset &preset = m_presetModel->add(currentPreset());
	m_ui->preset->setCurrentIndex(m_presetModel->indexOf(preset));
}

void InputSettings::renamePreset()
{
	const input::Preset *preset = currentPreset();
	if(preset) {
		bool ok;
		QString name = QInputDialog::getText(this, tr("Rename Preset"),
				tr("Preset name:"), QLineEdit::Normal, preset->name, &ok);
		input::Preset *mutablePreset = mutableCurrentPreset();
		if(preset == mutablePreset && ok && !name.isEmpty()) {
			m_presetModel->rename(*mutablePreset, name);
		}
	}
}

void InputSettings::removePreset()
{
	const input::Preset *preset = currentPreset();
	if(preset) {
		m_presetModel->remove(*preset);
	}
}

void InputSettings::presetDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
	int i = m_ui->preset->currentIndex();
	if(i >= 0 && i >= topLeft.row() && i <= bottomRight.row()) {
		const input::Preset *preset = currentPreset();
		if(preset) {
			updatePresetMenu();
		}
	}
}


void InputSettings::applyPresetToUi(const input::Preset &preset)
{
	if(!m_updateInProgress) {
		m_updateInProgress = true;
		m_ui->smoothing->setValue(preset.smoothing);
		m_ui->pressuresrc->setCurrentIndex(preset.pressureMode);
		m_ui->stackedWidget->setCurrentIndex(m_ui->pressuresrc->currentIndex());
		m_ui->stylusCurve->setCurve(preset.stylusCurve);
		m_ui->distanceCurve->setCurve(preset.distanceCurve);
		m_ui->velocityCurve->setCurve(preset.velocityCurve);
		m_ui->distance->setValue(preset.distance);
		m_ui->velocity->setValue(preset.velocity);
		m_updateInProgress = false;
		updateSmoothing();
		updatePressureMapping();
	}
}

void InputSettings::updatePresetMenu() const
{
	m_removePresetAction->setEnabled(m_presetModel->size() > 1);
}

void InputSettings::applyUiToPreset(input::Preset &preset) const
{
	if(!m_updateInProgress) {
		m_presetModel->apply(preset, m_ui->smoothing->value(),
				m_ui->pressuresrc->currentIndex(), m_ui->stylusCurve->curve(),
				m_ui->distanceCurve->curve(), m_ui->velocityCurve->curve(),
				m_ui->distance->value(), m_ui->velocity->value());
	}
}

}
