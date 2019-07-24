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

#include <QAbstractListModel>

namespace brushes {

class ClassicBrush;

class BrushPresetModel : public QAbstractListModel {
	Q_OBJECT
public:
	enum BrushPresetRoles {
		BrushPresetRole = Qt::UserRole + 1
	};

	static BrushPresetModel *getSharedInstance();

	explicit BrushPresetModel(QObject *parent=nullptr);
	~BrushPresetModel();

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QMap<int,QVariant> itemData(const QModelIndex &index) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role=Qt::EditRole) override;
	bool insertRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	bool removeRows(int row, int count, const QModelIndex &parent=QModelIndex()) override;
	Qt::DropActions supportedDropActions() const override;

	void addBrush(const ClassicBrush &brush);

private:
	void loadBrushes();

	struct Private;
	Private *d;
};

}

#endif
