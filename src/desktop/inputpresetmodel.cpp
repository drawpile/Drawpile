/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014-2020 Calle Laakkonen

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
#include "inputpresetmodel.h"

#include <QDebug>
#include <QUuid>

namespace input {
const int PresetModel::SMOOTHING_DEFAULT = 5;
const int PresetModel::DISTANCE_DEFAULT = 105;
const int PresetModel::VELOCITY_DEFAULT = 55;
}

namespace {

class PresetCreator {
public:
	virtual QString uuid() { return ""; }
	virtual QString name() { return ""; }

	virtual int smoothing() { return input::PresetModel::SMOOTHING_DEFAULT; }
	virtual int pressureMode() { return 0; }

	virtual bool hasStylusCurve() { return false; }
	virtual bool hasDistanceCurve() { return false; }
	virtual bool hasVelocityCurve() { return false; }

	virtual QString stylusCurve() { return ""; }
	virtual QString distanceCurve() { return ""; }
	virtual QString velocityCurve() { return ""; }

	virtual int distance() { return input::PresetModel::DISTANCE_DEFAULT; }
	virtual int velocity() { return input::PresetModel::VELOCITY_DEFAULT; }

	void create(input::Preset &preset, int i);
};

class SettingsPresetCreator : public PresetCreator {
public:
	explicit SettingsPresetCreator(QSettings &cfg) : PresetCreator{}, m_cfg{cfg} {}

	QString uuid() override { return m_cfg.value("uuid").toString(); }
	QString name() override { return m_cfg.value("name").toString(); }

	virtual int smoothing() { return m_cfg.value("smoothing", PresetCreator::smoothing()).toInt(); }
	virtual int pressureMode() { return m_cfg.value("pressuremode", PresetCreator::pressureMode()).toInt(); }

	virtual bool hasStylusCurve() { return m_cfg.contains("pressurecurve"); }
	virtual bool hasDistanceCurve() { return m_cfg.contains("distancecurve"); }
	virtual bool hasVelocityCurve() { return m_cfg.contains("velocitycurve"); }

	virtual QString stylusCurve() { return m_cfg.value("pressurecurve").toString(); }
	virtual QString distanceCurve() { return m_cfg.value("distancecurve").toString(); }
	virtual QString velocityCurve() { return m_cfg.value("velocitycurve").toString(); }

	virtual int distance() { return m_cfg.value("distance", PresetCreator::distance()).toInt(); }
	virtual int velocity() { return m_cfg.value("velocity", PresetCreator::velocity()).toInt(); }

private:
	QSettings &m_cfg;
};

class PresetCreatorFrom : public PresetCreator {
public:
	PresetCreatorFrom(const input::Preset &from) : PresetCreator{}, m_from{from} {}

	virtual int smoothing() { return m_from.smoothing; }
	virtual int pressureMode() { return m_from.pressureMode; }

	virtual bool hasStylusCurve() { return true; }
	virtual bool hasDistanceCurve() { return true; }
	virtual bool hasVelocityCurve() { return true; }

	virtual QString stylusCurve() { return m_from.stylusCurve.toString(); }
	virtual QString distanceCurve() { return m_from.distanceCurve.toString(); }
	virtual QString velocityCurve() { return m_from.velocityCurve.toString(); }

	virtual int distance() { return m_from.distance; }
	virtual int velocity() { return m_from.velocity; }

private:
	const input::Preset &m_from;
};

void PresetCreator::create(input::Preset &preset, int i)
{
	preset.uuid = uuid();
	if(preset.uuid.isEmpty()) {
		preset.uuid = QUuid::createUuid().toString();
	}

	preset.name = name();
	if(preset.name.isEmpty()) {
		preset.name = QString("%1 %2").arg("Preset", QString::number(i + 1));
	}

	preset.smoothing = smoothing();
	preset.pressureMode = pressureMode();

	if(hasStylusCurve()) {
		preset.stylusCurve.fromString(stylusCurve());
	}

	if(hasDistanceCurve()) {
		preset.distanceCurve.fromString(distanceCurve());
	}

	if(hasVelocityCurve()) {
		preset.velocityCurve.fromString(velocityCurve());
	}

	preset.distance = distance();
	preset.velocity = velocity();
}

}

namespace input {

PressureMapping Preset::getPressureMapping() const
{
	PressureMapping pm;
	switch(pressureMode) {
	case 0:
		pm.mode = PressureMapping::STYLUS;
		pm.curve = stylusCurve;
		pm.param = 0;
		break;
	case 1:
		pm.mode = PressureMapping::DISTANCE;
		pm.curve = distanceCurve;
		pm.param = distance;
		break;
	case 2:
		pm.mode = PressureMapping::VELOCITY;
		pm.curve = velocityCurve;
		pm.param = velocity;
		break;
	}
	return pm;
}

PresetModel::PresetModel(QObject *parent) :
	QAbstractListModel{parent},
	m_presets{}
{
	restoreSettings();
}

PresetModel::~PresetModel()
{
	saveSettings();
}

int PresetModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_presets.size();
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_presets.size() && role == Qt::DisplayRole) {
		return m_presets.at(index.row()).name;
	}
	return QVariant();
}


Preset *PresetModel::operator[](int i)
{
	return i >= 0 && i < m_presets.size() ? &m_presets[i] : nullptr;
}

const Preset *PresetModel::at(int i) const
{
	return i >= 0 && i < m_presets.size() ? &m_presets.at(i) : nullptr;
}

int PresetModel::indexOf(const Preset &preset) const
{
	int size = m_presets.size();
	for(int i = 0; i < size; ++i) {
		if(&preset == &m_presets.at(i)) {
			return i;
		}
	}
	return -1;
}

int PresetModel::size() const
{
	return m_presets.size();
}


int PresetModel::searchIndexByUuid(const QString &uuid) const
{
	int size = m_presets.size();
	for(int i = 0; i < size; ++i) {
		if(m_presets.at(i).uuid == uuid) {
			return i;
		}
	}
	return -1;
}

const Preset *PresetModel::searchPresetByUuid(const QString &uuid) const
{
	int i = searchIndexByUuid(uuid);
	return i >= 0 ? &m_presets.at(i) : nullptr;
}


Preset &PresetModel::add(const Preset *from)
{
	int size = m_presets.size();
	beginInsertRows(QModelIndex{}, size, size);
	m_presets.resize(size + 1);
	Preset &preset = m_presets[size];
	if(from) {
		createPresetFrom(preset, size, *from);
	} else {
		createBlankPreset(preset, size);
	}
	endInsertRows();
	return preset;
}

void PresetModel::createPresetFrom(Preset &preset, int i, const Preset &from)
{
	PresetCreatorFrom(from).create(preset, i);
}

void PresetModel::remove(const Preset &preset)
{
	int indexToRemove = indexOf(preset);
	if(indexToRemove >= 0 && size() > 1) {
		QString uuid = preset.uuid;
		beginRemoveRows(QModelIndex{}, indexToRemove, indexToRemove);
		m_presets.removeAt(indexToRemove);
		endRemoveRows();
		emit presetRemoved(uuid);
	}
}

void PresetModel::rename(Preset &preset, const QString &name)
{
	int indexToRename = indexOf(preset);
	if(indexToRename >= 0) {
		preset.name = name;
		emitChangeAt(indexToRename);
	}
}

void PresetModel::emitChangeAt(int i)
{
	if(i >= 0 && i < m_presets.size()) {
		QModelIndex qmi = index(i);
		emit dataChanged(qmi, qmi);
	}
}

void PresetModel::apply(Preset &preset, int smoothing, int pressureMode,
		const KisCubicCurve &stylusCurve, const KisCubicCurve &distanceCurve,
		const KisCubicCurve &velocityCurve, int distance, int velocity)
{
	preset.smoothing = smoothing;
	preset.pressureMode = pressureMode;
	preset.stylusCurve = stylusCurve;
	preset.distanceCurve = distanceCurve;
	preset.velocityCurve = velocityCurve;
	preset.distance = distance;
	preset.velocity = velocity;
	emit presetChanged(preset);
}


void PresetModel::restoreSettings()
{
	beginResetModel();
	m_presets.clear();

	QSettings cfg;
	restorePresetsSettings(cfg);

	if (m_presets.isEmpty()) {
		restoreLegacySettings(cfg);
	}

	endResetModel();
}

void PresetModel::restorePresetsSettings(QSettings &cfg)
{
	int size = cfg.beginReadArray("inputpresets");
	m_presets.resize(size);
	for(int i = 0; i < size; ++i) {
		cfg.setArrayIndex(i);
		restorePreset(cfg, m_presets[i], i);
	}
	cfg.endArray();
}

void PresetModel::restoreLegacySettings(QSettings &cfg)
{
	m_presets.resize(1);
	cfg.beginGroup("input");
	restorePreset(cfg, m_presets[0], 0);
	cfg.endGroup();
}

void PresetModel::restorePreset(QSettings &cfg, Preset &preset, int i)
{
	SettingsPresetCreator{cfg}.create(preset, i);
}

Preset &PresetModel::firstPresetOrCreateBlank()
{
	if(m_presets.isEmpty()) {
		m_presets.resize(1);
		createBlankPreset(m_presets[0], 0);
	}
	return m_presets[0];
}

void PresetModel::createBlankPreset(Preset &preset, int i)
{
	PresetCreator{}.create(preset, i);
}


void PresetModel::saveSettings()
{
	QSettings cfg;

	cfg.beginGroup("input");
	savePreset(cfg, firstPresetOrCreateBlank(), true);
	cfg.endGroup();

	int size = m_presets.size();
	cfg.beginWriteArray("inputpresets", size);
	for(int i = 0; i < size; ++i) {
		cfg.setArrayIndex(i);
		savePreset(cfg, m_presets.at(i), false);
	}
	cfg.endArray();
}

void PresetModel::savePreset(QSettings &cfg, const Preset &preset, bool legacy)
{
	if(!legacy) {
		cfg.setValue("uuid", preset.uuid);
		cfg.setValue("name", preset.name);
	}
	cfg.setValue("smoothing", preset.smoothing);
	cfg.setValue("pressuremode", preset.pressureMode);
	cfg.setValue("pressurecurve", preset.stylusCurve.toString());
	cfg.setValue("distancecurve", preset.distanceCurve.toString());
	cfg.setValue("velocitycurve", preset.velocityCurve.toString());
	cfg.setValue("distance", preset.distance);
	cfg.setValue("velocity", preset.velocity);
}

}
