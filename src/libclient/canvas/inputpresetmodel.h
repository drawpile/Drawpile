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
#ifndef PresetModel_H
#define PresetModel_H

#include "canvas/pressure.h"

#include <QAbstractListModel>
#include <QSettings>
#include <QVector>

namespace input {

struct Preset {
	//! A unique ID for the preset
	QString id;

	//! Human readable name of the preset
	QString name;

	int smoothing;
	PressureMapping curve;

	static Preset loadFromSettings(const QSettings &cfg);
	void saveToSettings(QSettings &cfg) const;
};

}

Q_DECLARE_TYPEINFO(input::Preset, Q_MOVABLE_TYPE);

namespace input {

class PresetModel : public QAbstractListModel {
	Q_OBJECT
public:
	explicit PresetModel(QObject *parent = nullptr);
	~PresetModel();

	PresetModel(const PresetModel &) = delete;
	PresetModel &operator=(const PresetModel &) = delete;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	bool removeRows(int row, int count, const QModelIndex &parent) override;

	const Preset *at(int i) const;
	int searchIndexById(const QString &id) const;
	const Preset *searchPresetById(const QString &id) const;

	void add(const Preset &preset);
	void update(int index, const Preset &preset);

signals:
	void presetChanged(const QString &id);

private:
	void restoreSettings();
	void saveSettings();

	QVector<Preset> m_presets;
};

}

Q_DECLARE_METATYPE(input::Preset)

#endif
