// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KEYFRAMELAYERMODEL_H
#define KEYFRAMELAYERMODEL_H

#include <QAbstractItemModel>
#include <QHash>
#include <QString>

struct KeyFrameLayerItem final {
	enum class Visibility { Default, Hidden, Revealed };
	int id;
	QString title;
	Visibility visibility;
	bool group;
	int children;
	int relIndex;
	int left;
	int right;
};

Q_DECLARE_TYPEINFO(KeyFrameLayerItem, Q_MOVABLE_TYPE);
Q_DECLARE_METATYPE(KeyFrameLayerItem)

class KeyFrameLayerModel final : public QAbstractItemModel {
	Q_OBJECT
public:
	enum Roles {
		IsVisibleRole = Qt::UserRole + 1,
	};

	KeyFrameLayerModel(
		const QVector<KeyFrameLayerItem> &items, QObject *parent = nullptr);

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

	Qt::ItemFlags flags(const QModelIndex &index) const override;

	QModelIndex parent(const QModelIndex &index) const override;

	QModelIndex index(
		int row, int column,
		const QModelIndex &parent = QModelIndex()) const override;

	QHash<int, bool> layerVisibility() const;

public slots:
	void toggleVisibility(int layerId, bool revealed);

private:
	void dataChangedRecursive(QModelIndex parent);

	QVector<KeyFrameLayerItem> m_items;
};

#endif
