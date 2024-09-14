// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CUSTOMSHORTCUTMODEL_H
#define LIBCLIENT_UTILS_CUSTOMSHORTCUTMODEL_H
#include <QAbstractTableModel>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QSet>

struct CustomShortcut {
	QString name;
	QString title;
	QKeySequence defaultShortcut;
	QKeySequence defaultAlternateShortcut;
	QKeySequence alternateShortcut;
	QKeySequence currentShortcut;

	bool operator<(const CustomShortcut &other) const
	{
		return title.compare(other.title) < 0;
	}
};

class CustomShortcutModel final : public QAbstractTableModel {
	Q_OBJECT
public:
	enum Column {
		Action = 0,
		CurrentShortcut,
		AlternateShortcut,
		DefaultShortcut,
		ColumnCount
	};

	explicit CustomShortcutModel(QObject *parent = nullptr);

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

	QVector<CustomShortcut>
	getShortcutsMatching(const QKeySequence &keySequence);

	void loadShortcuts(const QVariantMap &cfg);
	[[nodiscard]] QVariantMap saveShortcuts();

	void updateShortcuts();

	static QList<QKeySequence> getDefaultShortcuts(const QString &name);
	static void registerCustomizableAction(
		const QString &name, const QString &title,
		const QKeySequence &defaultShortcut,
		const QKeySequence &defaultAlternateShortcut);
	static void changeDisabledActionNames(
		const QVector<QPair<QString, bool>> &nameDisabledPairs);

private:
	void updateShortcutsInternal();
	void updateConflictRows();

	QVector<int> m_shortcutIndexes;
	QVector<CustomShortcut> m_loadedShortcuts;
	QSet<int> m_conflictRows;

	static QMap<QString, CustomShortcut> m_customizableActions;
	static QSet<QString> m_disabledActionNames;
};

#endif
