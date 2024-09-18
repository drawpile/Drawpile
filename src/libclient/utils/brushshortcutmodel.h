// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_BRUSHSHORTCUTMODEL_H
#define LIBCLIENT_UTILS_BRUSHSHORTCUTMODEL_H
#include "libclient/brushes/brushpresetmodel.h"
#include <QAbstractTableModel>
#include <QKeySequence>
#include <QPixmap>
#include <QSet>
#include <QSortFilterProxyModel>
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

	const QKeySequence &shortcutAt(int row);
	const QVector<int> &tagIdsAt(int row);

	QModelIndex indexById(int presetId, int column = 0) const;

	bool hasConflicts() const { return !m_conflictRows.isEmpty(); }

	void
	setExternalKeySequences(const QSet<QKeySequence> &externalKeySequences);

private:
	void updateConflictRows(bool emitDataChanges);

	brushes::BrushPresetModel *m_presetModel;
	QSet<int> m_conflictRows;
	QSet<QKeySequence> m_externalKeySequences;
	QSize m_iconSize;
	QVector<brushes::ShortcutPreset> m_presets;
};

// Note: Do not use setFilterFixedString with this model, use setSearchString
// instead. QSortFilterProxyModel does not expose the filter string properly.
class BrushShortcutFilterProxyModel : public QSortFilterProxyModel {
	Q_OBJECT
public:
	BrushShortcutFilterProxyModel(
		brushes::BrushPresetTagModel *tagModel, QObject *parent = nullptr);

	void setCurrentTagRow(int tagRow);
	void setSearchAllTags(bool searchAllTags);
	void setSearchString(const QString &searchString);

protected:
	bool filterAcceptsRow(
		int sourceRow, const QModelIndex &sourceParent) const override;

private:
	brushes::BrushPresetTagModel *m_tagModel;
	int m_tagRow = 0;
	bool m_searchAllTags = false;
	bool m_haveSearch = false;
};

#endif
