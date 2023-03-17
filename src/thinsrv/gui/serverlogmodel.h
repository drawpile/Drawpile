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

#ifndef SERVERLOGMODEL_H
#define SERVERLOGMODEL_H

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class ServerLogModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit ServerLogModel(QObject *parent=nullptr);

	//! Get the timestamp of the latest log entry
	QString lastTimestamp() const;

	void addLogEntry(const QJsonObject &entry);
	void addLogEntries(const QJsonArray &entries);

	QVariant data(const QModelIndex &index, int role) const override;

};

}
}

#endif

