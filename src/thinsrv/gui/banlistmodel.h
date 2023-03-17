/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2017-2018 Calle Laakkonen

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

#include "thinsrv/gui/jsonlistmodel.h"

namespace server {
namespace gui {

class BanListModel final : public JsonListModel
{
	Q_OBJECT
public:
	explicit BanListModel(QObject *parent=nullptr);

	void addBanEntry(const QJsonObject &entry);
	void removeBanEntry(int id);

	QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

protected:
	QVariant getData(const QString &key, const QJsonObject &obj) const override;
};

}
}

#endif
