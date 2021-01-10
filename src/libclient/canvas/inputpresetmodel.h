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
#ifndef PresetModel_H
#define PresetModel_H

#include "canvas/pressure.h"

#include <QAbstractListModel>
#include <QSettings>
#include <QVector>

namespace input {

struct Preset {
	QString uuid;
	QString name;
	int smoothing;
	int pressureMode;
	KisCubicCurve stylusCurve;
	KisCubicCurve distanceCurve;
	KisCubicCurve velocityCurve;
	int distance;
	int velocity;

	PressureMapping getPressureMapping() const;
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

	Preset *operator[](int i);
	const Preset *at(int i) const;
	int indexOf(const Preset &preset) const;
	int size() const;

	int searchIndexByUuid(const QString &uuid) const;
	const Preset *searchPresetByUuid(const QString &uuid) const;

	Preset &add(const Preset *from = nullptr);
	void remove(const Preset &preset);
	void rename(Preset &preset, const QString &name);
	void apply(Preset &preset, int smoothing, int pressureMode,
			const KisCubicCurve &stylusCurve, const KisCubicCurve &distanceCurve,
			const KisCubicCurve &velocityCurve, int distance, int velocity);

	static const int SMOOTHING_DEFAULT;
	static const int DISTANCE_DEFAULT;
	static const int VELOCITY_DEFAULT;

signals:
	void presetRemoved(const QString &uuid);
	void presetChanged(const Preset &preset);

private:
	void restoreSettings();
	void saveSettings();

	void createPresetFrom(Preset &preset, int i, const Preset &from);
	void emitChangeAt(int i);

	void restorePresetsSettings(QSettings &cfg);
	void restoreLegacySettings(QSettings &cfg);
	void restorePreset(QSettings &cfg, Preset &preset, int i);
	Preset &firstPresetOrCreateBlank();
	void createBlankPreset(Preset &preset, int i);

	void savePreset(QSettings &cfg, const Preset &preset, bool legacy);

	QVector<Preset> m_presets;
};

}

Q_DECLARE_METATYPE(input::Preset)

#endif
