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

#ifndef SERVERLOGMODEL_H
#define SERVERLOGMODEL_H

#include <QAbstractTableModel>
#include <QJsonObject>
#include <QList>

namespace server {
namespace gui {

class ServerLogModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	explicit ServerLogModel(QObject *parent=nullptr);

	//! Get the timestamp of the latest log entry
	QString lastTimestamp() const;

	void addLogEntry(const QJsonObject &entry);
	void addLogEntries(const QJsonArray &entries);

	int rowCount(const QModelIndex &parent) const override;
	int columnCount(const QModelIndex &parent) const override;
	QVariant data(const QModelIndex &index, int role) const override;
	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
	QList<QJsonObject> m_log;
};

}
}

#endif

