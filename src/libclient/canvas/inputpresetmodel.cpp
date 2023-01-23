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
#include "libclient/canvas/inputpresetmodel.h"
#include "libshared/util/ulid.h"

namespace input {

PresetModel::PresetModel(QObject *parent) :
	QAbstractListModel{parent},
	m_presets{}
{
}

PresetModel *PresetModel::getSharedInstance()
{
	static PresetModel *instance;
	if(!instance) {
		instance = new PresetModel;
		instance->restoreSettings();
	}
	return instance;
}

int PresetModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_presets.size();
}

QVariant PresetModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_presets.size()) {
		switch(role) {
		case Qt::DisplayRole:
		case Qt::EditRole:
			return m_presets.at(index.row()).name;
		}
	}
	return QVariant();
}

bool PresetModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
	if(role==Qt::EditRole && index.isValid() && index.row() >= 0 && index.row() < m_presets.size()) {
		const QString newname = value.toString().trimmed();

		if(!newname.isEmpty()) {
			auto &p = m_presets[index.row()];
			if(p.name != newname) {
				p.name = newname;
				emit dataChanged(index, index);
				return true;
			}
		}
	}

	return false;
}

Qt::ItemFlags PresetModel::flags(const QModelIndex &index) const
{
	Qt::ItemFlags f = Qt::NoItemFlags;
	if(index.isValid() && index.row() >= 0 && index.row() < m_presets.size()) {
		f = Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable;
	}

	return f;
}

bool PresetModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid() || row < 0 || row+count > m_presets.size())
		return false;

	beginRemoveRows(parent, row, row+count-1);
	m_presets.remove(row, count);
	endRemoveRows();
	return true;
}

const Preset *PresetModel::at(int i) const
{
	return i >= 0 && i < m_presets.size() ? &m_presets.at(i) : nullptr;
}

int PresetModel::searchIndexById(const QString &id) const
{
	for(int i = 0; i < m_presets.size(); ++i) {
		if(m_presets.at(i).id == id) {
			return i;
		}
	}
	return -1;
}

const Preset *PresetModel::searchPresetById(const QString &id) const
{
	const int i = searchIndexById(id);
	return i >= 0 ? &m_presets.at(i) : nullptr;
}


void PresetModel::add(const Preset &preset)
{
	int size = m_presets.size();
	beginInsertRows(QModelIndex{}, size, size);
	m_presets.append(preset);
	if(preset.id.isEmpty())
		m_presets.last().id = Ulid::make().toString();
	endInsertRows();
}

void PresetModel::update(int index, const Preset &preset)
{
	if(index < 0 || index >= m_presets.size())
		return;

	m_presets[index] = preset;
	const QModelIndex idx = this->index(index);
	emit dataChanged(idx, idx);
	emit presetChanged(preset.id);
}

void PresetModel::restoreSettings()
{
	beginResetModel();
	QSettings cfg;
	const int size = cfg.beginReadArray("inputpresets");
	m_presets.resize(size);
	for(int i = 0; i < size; ++i) {
		cfg.setArrayIndex(i);
		m_presets[i] = Preset::loadFromSettings(cfg);
	}
	cfg.endArray();

	if (m_presets.isEmpty()) {
		m_presets
		<< Preset {
			Ulid::make().toString(),
			"Stylus",
			8,
			PressureMapping {
				PressureMapping::STYLUS,
				KisCubicCurve(),
				1.0
			}
		}
		<< Preset {
			Ulid::make().toString(),
			"Distance",
			8,
			PressureMapping {
				PressureMapping::DISTANCE,
				KisCubicCurve(),
				1.0
			}
		}
		<< Preset {
			Ulid::make().toString(),
			"Velocity",
			8,
			PressureMapping {
				PressureMapping::VELOCITY,
				KisCubicCurve(),
				1.0
			}
		};
	}

	endResetModel();
}

void PresetModel::saveSettings()
{
	QSettings cfg;

	const int size = m_presets.size();
	cfg.beginWriteArray("inputpresets", size);
	for(int i = 0; i < size; ++i) {
		cfg.setArrayIndex(i);
		m_presets.at(i).saveToSettings(cfg);
	}
	cfg.endArray();
}


Preset Preset::loadFromSettings(const QSettings &cfg)
{
	Preset p;
	p.id = cfg.value("id").toString();
	if(p.id.isEmpty())
		p.id = Ulid::make().toString();
	p.name = cfg.value("name").toString();
	p.smoothing = cfg.value("smoothing").toInt();
	p.curve.mode = PressureMapping::Mode(cfg.value("mode").toInt());
	p.curve.param = cfg.value("param").toReal();
	p.curve.curve.fromString(cfg.value("curve").toString());
	return p;
}

void Preset::saveToSettings(QSettings &cfg) const
{
	cfg.setValue("id", id);
	cfg.setValue("name", name);
	cfg.setValue("smoothing", smoothing);
	cfg.setValue("mode", curve.mode);
	cfg.setValue("param", curve.param);
	cfg.setValue("curve", curve.curve.toString());
}

}
