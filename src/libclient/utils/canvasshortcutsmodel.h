// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef CANVASSHORTCUTSMODEL_H
#define CANVASSHORTCUTSMODEL_H

#include "libclient/canvas/canvasshortcuts.h"
#include <QAbstractTableModel>
#include <QVector>

class CanvasShortcutsModel : public QAbstractTableModel {
	Q_OBJECT
public:
	explicit CanvasShortcutsModel(QObject *parent = nullptr);

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
