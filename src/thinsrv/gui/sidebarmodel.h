// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SIDEBARMODEL_H
#define SIDEBARMODEL_H

#include "thinsrv/gui/pagefactory.h"

#include <QAbstractItemModel>
#include <QList>

namespace server {
namespace gui {

class SidebarModel final : public QAbstractItemModel
{
	Q_OBJECT
public:
	enum SidebarRoles {
		PageFactoryRole = Qt::UserRole + 1
	};

	SidebarModel(QObject *parent=nullptr);
	~SidebarModel() override;

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
