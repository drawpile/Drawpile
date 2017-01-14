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

#include "announcementdialog.h"
#include "utils/listservermodel.h"
#include "ui_announcementdialog.h"

#include <QDebug>
#include <QStringListModel>
#include <QIdentityProxyModel>
#include <QMenu>

namespace dialogs {

class ListingProxyModel : public QIdentityProxyModel {
public:
	ListingProxyModel(QObject *parent=nullptr)
		: QIdentityProxyModel(parent)
	{
		sessionlisting::ListServerModel servermodel(false);
		for(const sessionlisting::ListServer &s : servermodel.servers()) {
			m_servers[s.url] = QPair<QIcon,QString>(s.icon, s.name);
		}
	}

	QVariant data(const QModelIndex &proxyIndex, int role) const override
	{
		QVariant data = QAbstractProxyModel::data(proxyIndex, role);
		if(role == Qt::DisplayRole && data.isValid()) {
			QString url = data.toString();
			if(m_servers.contains(url))
				return m_servers[url].second;

		} else if(role == Qt::DecorationRole) {
			QString url = QAbstractProxyModel::data(proxyIndex, Qt::DisplayRole).toString();
			if(m_servers.contains(url))
				return m_servers[url].first;
		} else if(role == Qt::UserRole) {
			return QAbstractProxyModel::data(proxyIndex, Qt::DisplayRole);
		}

		return data;
	}

	Qt::ItemFlags flags(const QModelIndex &index) const override
	{
		if(index.isValid())
			return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
		return 0;
	}

	QHash<QString,QPair<QIcon,QString>> m_servers;
};

AnnouncementDialog::AnnouncementDialog(QStringListModel *model, bool isAdmin, QWidget *parent)
	: QDialog(parent), m_ui(new Ui_AnnouncementDialog)
{
	m_ui->setupUi(this);

	ListingProxyModel *proxy = new ListingProxyModel(this);
	proxy->setSourceModel(model);
	m_ui->listView->setModel(proxy);

	if(isAdmin) {
		QMenu *addMenu = new QMenu(this);

		QHashIterator<QString,QPair<QIcon,QString>> i(proxy->m_servers);
		while(i.hasNext()) {
			auto item = i.next();
			QAction *a = addMenu->addAction(item.value().first, item.value().second);
			a->setProperty("API_URL", item.key());
		}

		m_ui->addButton->setMenu(addMenu);

		connect(addMenu, &QMenu::triggered, [this](QAction *a) {
			QString apiUrl = a->property("API_URL").toString();
			qDebug() << "Requesting announcement:" << apiUrl;
			emit requestAnnouncement(apiUrl);
		});

		connect(m_ui->removeButton, &QPushButton::clicked, [this]() {
			auto sel = m_ui->listView->selectionModel()->selection();
			QString apiUrl;
			if(!sel.isEmpty())
				apiUrl = sel.first().indexes().first().data(Qt::UserRole).toString();
			if(!apiUrl.isEmpty()) {
				qDebug() << "Requesting unlisting:" << apiUrl;
				emit requestUnlisting(apiUrl);
			}
		});
	} else {
		m_ui->addButton->setEnabled(false);
		m_ui->removeButton->setEnabled(false);
	}
}

AnnouncementDialog::~AnnouncementDialog()
{
	delete m_ui;
}

}
