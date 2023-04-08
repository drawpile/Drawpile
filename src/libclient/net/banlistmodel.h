// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef BANLISTMODEL_H
#define BANLISTMODEL_H

#include <QAbstractTableModel>

class QJsonArray;

namespace net {

struct BanlistEntry {
	int id;
	QString username;
	QString ip;
	QString bannedBy;
};

/**
 * @brief A representation of the serverside banlist
 *
 * This is just for showing the list to the user
 */
class BanlistModel final : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit BanlistModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;

	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const override;

	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const override;

	//! Replace banlist content
	void updateBans(const QJsonArray &banlist);

	void clear();

private:
	QList<BanlistEntry> m_banlist;
};

}

#endif // BANLISTMODEL_H
