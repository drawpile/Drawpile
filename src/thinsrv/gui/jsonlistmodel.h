/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2018 Calle Laakkonen

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

#ifndef JSONLISTMODEL_H
#define JSONLISTMODEL_H

#include <QAbstractTableModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>

namespace server {
namespace gui {

/**
 * @brief A generic table model for JSON objects
 */
class JsonListModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	struct Column {
		QString key;
		QString title;
	};
	JsonListModel(const QVector<Column> &columns, QObject *parent=nullptr);

	void setList(const QJsonArray &list);
	QJsonObject objectAt(int row) const { return m_list.at(row).toObject(); }

	int rowCount(const QModelIndex &parent=QModelIndex()) const override;
	int columnCount(const QModelIndex &parent=QModelIndex()) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

protected:
	virtual QVariant getData(const QString &key, const QJsonObject &obj) const;
	virtual Qt::ItemFlags getFlags(const QJsonObject &obj) const;
	int getId(const QJsonObject &obj) const;

	QJsonArray m_list;
	QVector<Column> m_columns;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
	class [[maybe_unused]] AbstractJsonListModelMarker : JsonListModel
	{
		inline Qt::ItemFlags getFlags(const QJsonObject &) const override { return Qt::NoItemFlags; }
		QVariant getData(const QString &, const QJsonObject &) const override { return QVariant(); }
	};
}

}
}

#endif

