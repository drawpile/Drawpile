// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef PresetModel_H
#define PresetModel_H

#include "libclient/canvas/pressure.h"

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

class PresetModel final : public QAbstractListModel {
	Q_OBJECT
public:
	explicit PresetModel(QObject *parent = nullptr);

	PresetModel(const PresetModel &) = delete;
	PresetModel &operator=(const PresetModel &) = delete;

	static PresetModel *getSharedInstance();

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

	void restoreSettings();
	void saveSettings();

signals:
	void presetChanged(const QString &id);

private:
	QVector<Preset> m_presets;
};

}

Q_DECLARE_METATYPE(input::Preset)

#endif
