// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_BRUSHSHORTCUTMODEL_H
#define LIBCLIENT_UTILS_BRUSHSHORTCUTMODEL_H
#include <QAbstractTableModel>
#include <QKeySequence>
#include <QPixmap>
#include <QVector>

namespace brushes {
class BrushPresetModel;
}

class BrushShortcutModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column { PresetName = 0, Shortcut, ColumnCount };
	enum Role { FilterRole = Qt::UserRole + 1 };

	explicit BrushShortcutModel(
		brushes::BrushPresetModel *presetModel, QSize iconSize,
		QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	bool
	setData(const QModelIndex &index, const QVariant &value, int role) override;

	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

private:
	struct Entry {
		int presetId;
		QString presetName;
		QPixmap presetThumbnail;
		QKeySequence shortcut;
	};

	void addEntry(
		int presetId, const QString &presetName, const QKeySequence &shortcut);

	brushes::BrushPresetModel *m_presetModel;
	QSize m_iconSize;
	QVector<Entry> m_entries;
};

#endif
