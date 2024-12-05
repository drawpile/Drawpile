// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_HOSTPRESETMODEL_H
#define LIBCLIENT_UTILS_HOSTPRESETMODEL_H
#include "libclient/utils/statedatabase.h"
#include <QAbstractItemModel>
#include <QJsonObject>
#include <QSet>

namespace utils {

class StateDatabase;

class HostPresetModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	enum Roles {
		IdRole = Qt::UserRole + 1,
		VersionRole,
		TitleRole,
		DataRole,
		SortRole,
		DefaultRole,
	};

	HostPresetModel(StateDatabase &state, QObject *parent = nullptr);

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &child) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	QSet<int> getPresetIdsByTitle(const QString &title) const;

	static QSet<int>
	getPresetIdsByTitleWith(StateDatabase &state, const QString &title);

	bool renamePresetById(int id, const QString &title);
	bool deletePresetById(int id);

	static void
	migrateOldPresets(StateDatabase &state, const QString &oldPresetsPath);

private:
	struct Preset {
		int id;
		int version;
		QString title;
		QJsonObject data;
	};

	static bool
	loadOldPresets(StateDatabase::Query &qry, const QString &oldPresetsPath);

	static bool loadOldPreset(const QString &path, QByteArray &outData);

	void loadPresets();

	StateDatabase &m_state;
	QVector<Preset> m_presets;
};

}

#endif
