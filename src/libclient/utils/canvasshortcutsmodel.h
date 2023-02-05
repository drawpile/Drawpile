/*
 * Drawpile - a collaborative drawing program.
 *
 * Copyright (C) 2023 askmeaboutloom
 *
 * Drawpile is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Drawpile is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Drawpile.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef CANVASSHORTCUTSMODEL_H
#define CANVASSHORTCUTSMODEL_H

#include "canvas/canvasshortcuts.h"
#include <QAbstractTableModel>
#include <QVector>

class CanvasShortcutsModel : public QAbstractTableModel {
	Q_OBJECT
public:
	explicit CanvasShortcutsModel(QObject *parent = nullptr);

	~CanvasShortcutsModel();

	void loadShortcuts(QSettings &cfg);
	void saveShortcuts(QSettings &cfg);
	void restoreDefaults();

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	bool removeRows(
		int row, int count, const QModelIndex &parent = QModelIndex()) override;

	QVariant headerData(
		int section, Qt::Orientation orientation,
		int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

	const CanvasShortcuts::Shortcut *shortcutAt(int row) const;

	int addShortcut(const CanvasShortcuts::Shortcut &s);

	int editShortcut(
		const CanvasShortcuts::Shortcut &prev,
		const CanvasShortcuts::Shortcut &s);

	const CanvasShortcuts::Shortcut *searchConflict(
		const CanvasShortcuts::Shortcut &s,
		const CanvasShortcuts::Shortcut *except) const;

	static QString shortcutTitle(
		const CanvasShortcuts::Shortcut *s,
		bool actionAndFlagsOnly = false);

	static QString shortcutToString(
		unsigned int type, Qt::KeyboardModifiers mods,
		const QSet<Qt::Key> &keys, Qt::MouseButton button);

	bool hasChanges() const;

private:
	static QString mouseButtonToString(Qt::MouseButton button);
	static QString actionToString(const CanvasShortcuts::Shortcut &s);
	static QString flagsToString(const CanvasShortcuts::Shortcut &s);

	CanvasShortcuts m_canvasShortcuts;
	bool m_hasChanges;
};

#endif
