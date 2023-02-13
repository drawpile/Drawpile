/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2021 Calle Laakkonen

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

#include "inputsettings.h"
#include "canvas/inputpresetmodel.h"

#include "ui_inputcfg.h"

namespace widgets {

InputSettings::InputSettings(QWidget *parent) :
	Popup(parent),
	m_presetModel(input::PresetModel::getSharedInstance()),
	m_updateInProgress(false),
	m_indexChangeInProgress(false)
{
	m_ui = new Ui_InputSettings;
	m_ui->setupUi(this);

	m_presetmenu = new QMenu(this);
	m_presetmenu->addAction(tr("New"), this, &InputSettings::addPreset);
	m_presetmenu->addAction(tr("Duplicate"), this, &InputSettings::copyPreset);
	m_removePresetAction = m_presetmenu->addAction(tr("Delete"), this, &InputSettings::removePreset);
	m_ui->presetButton->setMenu(m_presetmenu);

	m_ui->preset->setModel(m_presetModel);
	m_ui->preset->setCurrentIndex(0);
	choosePreset(m_ui->preset->currentIndex());

	// Make connections
	connect(m_ui->smoothing, &QSlider::valueChanged, this, &InputSettings::applyUiToPreset);
	connect(m_ui->curve, &KisCurveWidget::curveChanged, this, &InputSettings::applyUiToPreset);
	connect(m_ui->curveParam, &QSlider::valueChanged, this, &InputSettings::applyUiToPreset);
	connect(m_ui->pressuresrc, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputSettings::applyUiToPreset);

	connect(m_ui->preset, &QComboBox::editTextChanged, this, &InputSettings::presetNameChanged);
	connect(m_ui->preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputSettings::choosePreset);
	connect(m_ui->preset, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &InputSettings::currentIndexChanged);
	connect(m_presetModel, &QAbstractItemModel::rowsInserted, this, &InputSettings::onPresetCountChanged);
	connect(m_presetModel, &QAbstractItemModel::rowsRemoved, this, &InputSettings::onPresetCountChanged);
	connect(m_presetModel, &QAbstractItemModel::modelReset, this, &InputSettings::onPresetCountChanged);

	onPresetCountChanged();
}

InputSettings::~InputSettings()
{
	m_presetModel->saveSettings();
	delete m_ui;
}

void InputSettings::setCurrentIndex(int index)
{
	if(!m_indexChangeInProgress && index >= 0 && index < m_ui->preset->count()) {
		m_indexChangeInProgress = true;
		m_ui->preset->setCurrentIndex(index);
		m_indexChangeInProgress = false;
	}
}

void InputSettings::setCurrentPreset(const QString &id)
{
	const int idx = m_presetModel->searchIndexById(id);
	if(idx>=0)
		m_ui->preset->setCurrentIndex(idx);
}

void InputSettings::choosePreset(int index)
{
	const input::Preset *preset = m_presetModel->at(index);
	if(preset) {
		applyPresetToUi(*preset);
	}
}

void InputSettings::presetNameChanged(const QString &name)
{
	auto *m = m_ui->preset->model();
	m->setData(m->index(m_ui->preset->currentIndex(), 0), name);
}

void InputSettings::addPreset()
{
	m_presetModel->add(input::Preset {
		QString(), // Generate a new id.
		tr("New"),
		8,
		PressureMapping {
			PressureMapping::STYLUS,
			KisCubicCurve(),
			1.0
		},
	});
	setCurrentIndex(m_presetModel->rowCount()-1);
}

void InputSettings::copyPreset()
{
	const input::Preset *current = m_presetModel->at(m_ui->preset->currentIndex());
	if(!current)
		return;
	input::Preset p = *current;
	p.id = QString();
	p.name = tr("New %1").arg(p.name);
	m_presetModel->add(p);
	setCurrentIndex(m_presetModel->rowCount()-1);
}

void InputSettings::removePreset()
{
	m_presetModel->removeRows(m_ui->preset->currentIndex(), 1, QModelIndex());
}

void InputSettings::onPresetCountChanged()
{
	m_removePresetAction->setEnabled(m_presetModel->rowCount() > 1);
}

void InputSettings::applyPresetToUi(const input::Preset &preset)
{
	if(!m_updateInProgress) {
		m_updateInProgress = true;
		m_ui->smoothing->setValue(preset.smoothing);
		m_ui->pressuresrc->setCurrentIndex(preset.curve.mode);
		m_ui->curve->setCurve(preset.curve.curve);
		m_ui->curveParam->setValue(int(preset.curve.param * 100.0));
		updateModeUi(preset.curve.mode);
		m_updateInProgress = false;
	}
}

void InputSettings::applyUiToPreset()
{
	if(!m_updateInProgress) {
		const input::Preset *current = m_presetModel->at(m_ui->preset->currentIndex());
		if(!current)
			return;

		m_presetModel->update(
			m_ui->preset->currentIndex(),
			input::Preset{
				current->id,
				current->name,
				m_ui->smoothing->value(),
				PressureMapping {
					PressureMapping::Mode(m_ui->pressuresrc->currentIndex()),
					m_ui->curve->curve(),
					qreal(m_ui->curveParam->value()) / 100.0
				}
			}
		);
		updateModeUi(current->curve.mode);
		emit activePresetModified(current);
	}
}

void InputSettings::updateModeUi(PressureMapping::Mode mode)
{
	const char *curveParam;
	switch(mode) {
	case PressureMapping::DISTANCE:
		curveParam = "Curve distance";
		break;
	case PressureMapping::VELOCITY:
		curveParam = "Velocity range";
		break;
	case PressureMapping::STYLUS:
	default:
		curveParam = nullptr;
		break;
	}
	m_ui->curveParam->setVisible(curveParam != nullptr);
#if QT_CONFIG(tooltip)
	m_ui->curveParam->setToolTip(QCoreApplication::translate(
			"InputSettings", curveParam, nullptr));
#endif
}

}
