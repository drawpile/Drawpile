/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2019 Calle Laakkonen

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

#ifndef DP_BRUSHPRESETMODEL_H
#define DP_BRUSHPRESETMODEL_H

#include <QAbstractItemModel>

namespace brushes {

class ClassicBrush;

/**
 * List of brush presets
 *
 * Brush presets are grouped into non-nestable folders. The first
 * folder is always named "Default". All unsorted brushes are put into
 * this folder. Note that folders do not necessarily correspond to any actual
 * filesystem directories, they are defined in the index file only.
 */
class BrushPresetModel : public QAbstractItemModel {
	Q_OBJECT
public:
	enum BrushPresetRoles {
		BrushPresetRole = Qt::UserRole + 1,
		BrushRole
	};

	static BrushPresetModel *getSharedInstance();

	explicit BrushPresetModel(QObject *parent=nullptr);
	~BrushPresetModel();

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;

	QModelIndex parent(const QModelIndex &index) const override;
	QModelIndex index(int row, int column=0, const QModelIndex &parent=QModelIndex()) const override;

	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QMap<int,QVariant> itemData(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role=Qt::EditRole) override;
	bool insertRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	bool removeRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	Qt::DropActions supportedDropActions() const override;

	void addBrush(int folderIndex, const ClassicBrush &brush);
	void addFolder(const QString &title);
	bool moveBrush(const QModelIndex &brushIndex, int targetFolder);

	// This should be private. Remove from here once brush migration support is removed.
	static bool writeBrush(const ClassicBrush &brush, const QString &filename);

private slots:
	void saveBrushes();

private:
	void loadBrushes();

	struct Private;
	Private *d;
};

}

#endif
