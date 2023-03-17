/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2020 Calle Laakkonen

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
#ifndef CUSTOMSHORTCUTMODEL_H
#define CUSTOMSHORTCUTMODEL_H

#include <QAbstractTableModel>
#include <QList>
#include <QMap>
#include <QKeySequence>

struct CustomShortcut {
	QString name;
	QString title;
	QKeySequence defaultShortcut;
	QKeySequence alternateShortcut;
	QKeySequence currentShortcut;

	bool operator<(const CustomShortcut &other) const { return title.compare(other.title) < 0; }
};

class CustomShortcutModel final : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit CustomShortcutModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role) override;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	void loadShortcuts();
	void saveShortcuts();

	static QKeySequence getDefaultShortcut(const QString &name);
	static void registerCustomizableAction(const QString &name, const QString &title, const QKeySequence &defaultShortcut);

private:
	QVector<CustomShortcut> m_shortcuts;

	static QMap<QString, CustomShortcut> m_customizableActions;
};

#endif // CUSTOMSHORTCUTMODEL_H
