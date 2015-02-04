/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
	QKeySequence currentShortcut;

	CustomShortcut() { }
	CustomShortcut(const QString &n, const QString &t, const QKeySequence &s)
		: name(n), title(t), defaultShortcut(s), currentShortcut(QKeySequence())
	{ }
};

class CustomShortcutModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit CustomShortcutModel(QObject *parent=0);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	int columnCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	bool setData(const QModelIndex &index, const QVariant &value, int role);
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	void saveShortcuts();

	static void registerCustomizableAction(const QString &name, const QString &title, const QKeySequence &defaultShortcut);
private:

	QList<CustomShortcut> _shortcuts;

	static QMap<QString, CustomShortcut> _customizableActions;
};

#endif // CUSTOMSHORTCUTMODEL_H
