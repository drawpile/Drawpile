/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2014 Calle Laakkonen

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

#ifndef PALETTELISTMODEL_H
#define PALETTELISTMODEL_H

#include <QAbstractListModel>
#include <QIcon>

class Palette;

class PaletteListModel : public QAbstractListModel
{
	Q_OBJECT
public:
	explicit PaletteListModel(QObject *parent = 0);

	static PaletteListModel *getSharedInstance();

	/**
	 * @brief Load palettes from standard paths
	 */
	void loadPalettes();

	/**
	 * @brief Save all changed palettes
	 */
	void saveChanged();

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);
	bool removeRows(int row, int count, const QModelIndex &parent);

	Palette *getPalette(int index);

	int findPalette(const QString &name) const;

	//! Create a new palette and append it to the list
	void addNewPalette();

	//! Duplicate the given palette
	bool copyPalette(int index);

private:
	bool isUniqueName(const QString &name, int exclude=-1) const;

	QList<Palette*> _palettes;
	QIcon _readonlyEmblem;
};

#endif // PALETTELISTMODEL_H
