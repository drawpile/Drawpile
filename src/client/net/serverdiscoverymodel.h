/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015 Calle Laakkonen

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
#ifndef SERVERDISCOVERYMODEL_H
#define SERVERDISCOVERYMODEL_H

#include "../shared/net/protover.h"

#include <QAbstractTableModel>
#include <QUrl>
#include <QDateTime>

#include <KDNSSD/DNSSD/RemoteService>

namespace KDNSSD {
	class ServiceBrowser;
}

struct DiscoveredServer {
	QUrl url;
	QString name;
	QString title;
	protocol::ProtocolVersion protocol;
	QDateTime started;
};

class ServerDiscoveryModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	ServerDiscoveryModel(QObject *parent=nullptr);

	int rowCount(const QModelIndex &parent=QModelIndex()) const;
	int columnCount(const QModelIndex &parent=QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role=Qt::DisplayRole) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role=Qt::DisplayRole) const;
	Qt::ItemFlags flags(const QModelIndex &index) const;

	//QUrl sessionUrl(int index) const;

	void discover();

private slots:
	void addService(KDNSSD::RemoteService::Ptr service);
	void removeService(KDNSSD::RemoteService::Ptr service);

private:
	QList<DiscoveredServer> _servers;

	KDNSSD::ServiceBrowser *_browser;
};

#endif

