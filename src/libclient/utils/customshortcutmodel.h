// SPDX-License-Identifier: GPL-3.0-or-later

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
