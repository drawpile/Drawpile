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
#include <QJsonArray>

namespace server {
namespace gui {

class BanListModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit BanListModel(QObject *parent=nullptr);

	void setBanList(const QJsonArray &sessions);
	void addBanEntry(const QJsonObject &entry);
	void removeBanEntry(int id);

	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const;

private:
	QJsonArray m_banlist;
};

}
}

#endif
