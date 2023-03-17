/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017 Calle Laakkonen

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
