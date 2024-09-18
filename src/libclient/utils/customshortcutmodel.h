// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBCLIENT_UTILS_CUSTOMSHORTCUTMODEL_H
#define LIBCLIENT_UTILS_CUSTOMSHORTCUTMODEL_H
#include <QAbstractTableModel>
#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QList>
#include <QMap>
#include <QSet>

struct CustomShortcut {
	QString name;
	QString title;
	QString searchText;
	QIcon icon;
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

	enum Role { FilterRole = Qt::UserRole + 1 };

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

	const CustomShortcut &shortcutAt(int row) const;

	bool hasConflicts() const { return !m_conflictRows.isEmpty(); }

	void
	setExternalKeySequences(const QSet<QKeySequence> &externalKeySequences);

	static QList<QKeySequence> getDefaultShortcuts(const QString &name);
	static bool isCustomizableActionRegistered(const QString &name);
	static void registerCustomizableAction(
		const QString &name, const QString &title, const QIcon &icon,
		const QKeySequence &defaultShortcut,
		const QKeySequence &defaultAlternateShortcut,
		const QString &searchText = QString());
	static void
	setCustomizableActionIcon(const QString &name, const QIcon &icon);
	static void changeDisabledActionNames(
		const QVector<QPair<QString, bool>> &nameDisabledPairs);

private:
	struct Conflict {
		bool current = false;
		bool alternate = false;

		bool operator==(Conflict &other) const
		{
			return current == other.current && alternate == other.alternate;
		}

		bool operator!=(const Conflict &other) const
		{
			return current != other.current || alternate != other.alternate;
		}

		void mergeWith(const Conflict &other)
		{
			current = current || other.current;
			alternate = alternate || other.alternate;
		}
	};

	void updateShortcutsInternal();
	void updateConflictRows(bool emitDataChanges);

	QVector<int> m_shortcutIndexes;
	QVector<CustomShortcut> m_loadedShortcuts;
	QHash<int, Conflict> m_conflictRows;
	QSet<QKeySequence> m_externalKeySequences;

	static QMap<QString, CustomShortcut> m_customizableActions;
	static QSet<QString> m_disabledActionNames;
};

#endif
