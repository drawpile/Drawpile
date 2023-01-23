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

#ifndef SIDEBARMODEL_H
#define SIDEBARMODEL_H

#include "thinsrv/gui/pagefactory.h"

#include <QAbstractItemModel>
#include <QList>

namespace server {
namespace gui {

class SidebarModel : public QAbstractItemModel
{
	Q_OBJECT
public:
	enum SidebarRoles {
		PageFactoryRole = Qt::UserRole + 1
	};

	SidebarModel(QObject *parent=nullptr);
	~SidebarModel();

	QVariant data(const QModelIndex &index, int role) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;
	QVariant headerData(int section, Qt::Orientation orientation,
						int role = Qt::DisplayRole) const override;
	QModelIndex index(int row, int column,
					  const QModelIndex &parent = QModelIndex()) const override;
	QModelIndex parent(const QModelIndex &index) const override;
	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int columnCount(const QModelIndex &parent = QModelIndex()) const override;

public slots:
	void setSessionList(const QJsonArray &sessions);

private:
	QList<PageFactory*> m_summarypages;
	QList<PageFactory*> m_sessions;
};

}
}

#endif
