/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2019 Calle Laakkonen

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

#include "listservermodel.h"
#include "../libshared/util/paths.h"

#include <QImage>
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QSslSocket>

namespace sessionlisting {

ListServerModel::ListServerModel(bool includeReadOnly, QObject *parent)
	: QAbstractListModel(parent)
{
	loadServers(includeReadOnly);
}

int ListServerModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid())
		return 0;
	return m_servers.size();
}

QVariant ListServerModel::data(const QModelIndex &index, int role) const
{
	if(index.isValid() && index.row() >= 0 && index.row() < m_servers.size()) {
		const ListServer &srv = m_servers.at(index.row());
		switch(role) {
		case Qt::DisplayRole: return srv.name;
		case Qt::DecorationRole: return srv.icon;
		case Qt::ToolTipRole: return QString("%1\n\n%2\n\nURL: %3\nRead-only: %4, public: %5, private: %6").arg(
			srv.name,
			srv.description,
			srv.url,
			srv.readonly ? QStringLiteral("yes") : QStringLiteral("no"),
			srv.publicListings ? QStringLiteral("yes") : QStringLiteral("no"),
			srv.privateListings ? QStringLiteral("yes") : QStringLiteral("no")
			);
		case UrlRole: return srv.url;
		case DescriptionRole: return srv.description;
		case PublicListRole: return srv.publicListings;
		case PrivateListRole: return srv.privateListings;
		}
	}
	return QVariant();
}

bool ListServerModel::moveRow(const QModelIndex &sourceParent, int sourceRow, const QModelIndex &destinationParent, int destinationChild)
{
	if(sourceParent.isValid() || destinationParent.isValid())
		return false;

	if(sourceRow < 0 || sourceRow >= m_servers.size())
		return false;

	const int destinationRow = qBound(0, destinationChild, m_servers.size()-1);

	if(sourceRow == destinationRow)
		return false;

	beginMoveRows(sourceParent, sourceRow, sourceRow, destinationParent, destinationRow + (destinationRow > sourceRow));
	m_servers.move(sourceRow, destinationRow);
	endMoveRows();
	return true;
}

bool ListServerModel::removeRows(int row, int count, const QModelIndex &parent)
{
	if(parent.isValid())
		return false;

	if(row<0 || count<=0 || row+count>m_servers.count())
		return false;

	beginRemoveRows(parent, row, row+count-1);
	while(count-->0) m_servers.removeAt(row);
	endRemoveRows();
	return true;
}

bool ListServerModel::addServer(const QString &name, const QString &url, const QString &description, bool readonly, bool pub, bool priv)
{
	const ListServer lstSrv {
			QIcon(),
			QString(),
			name,
			url,
			description,
			readonly,
			pub,
			priv
		};

	// First check if a server with this URL already exists
	for(int i=0;i<m_servers.size();++i) {
		if(m_servers.at(i).url == url) {
			// Already exists! Update data instead of adding
			m_servers[i] = lstSrv;
			const auto idx = index(i);
			emit dataChanged(idx, idx);
			return false;
		}
	}

	// No? Then add it
	beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
	m_servers << lstSrv;
	endInsertRows();
	return true;
}

bool ListServerModel::removeServer(const QString &url)
{
	// Find the server
	int found = -1;
	for(int i=0;i<m_servers.size();++i) {
		if(m_servers[i].url == url) {
			found = i;
			break;
		}
	}

	if(found < 0)
		return false;

	removeRow(found);

	return true;
}

void ListServerModel::setFavicon(const QString &url, const QImage &icon)
{
	// Find the server
	for(int i=0;i<m_servers.size();++i) {
		ListServer &s = m_servers[i];
		if(s.url != url)
			continue;

		// Make sure the icon is not huge
		QImage scaledIcon;

		if(icon.width() > 128 || icon.height() > 128)
			scaledIcon = icon.scaled(QSize(128, 128), Qt::KeepAspectRatio);
		else
			scaledIcon = icon;

		// Serialize icon in PNG format
		QByteArray data;
		QBuffer buffer(&data);
		buffer.open(QIODevice::WriteOnly);
		scaledIcon.save(&buffer, "PNG");

		// Generate file name
		QCryptographicHash hash(QCryptographicHash::Sha1);
		hash.addData(data);

		s.iconName = hash.result().toHex() + ".png";
		s.icon = QIcon(QPixmap::fromImage(scaledIcon));

		// Save file to disk
		QFile f(utils::paths::writablePath("favicons/", s.iconName));
		if(!f.open(QIODevice::WriteOnly)) {
			qWarning() << "Unable to open" << f.fileName() << f.errorString();
		} else {
			f.write(data);
			f.close();
		}
	}
}

QVector<ListServer> ListServerModel::listServers(bool includeReadOnly)
{
	QVector<ListServer> list;

	QSettings cfg;
	const QString iconPath = utils::paths::writablePath("favicons/");
	const int count = cfg.beginReadArray("listservers");
	for(int i=0;i<count;++i) {
		cfg.setArrayIndex(i);
		ListServer ls {
			QIcon(),
			cfg.value("icon").toString(),
			cfg.value("name").toString(),
			cfg.value("url").toString(),
			cfg.value("description").toString(),
			cfg.value("readonly").toBool(),
			cfg.value("public", true).toBool(),
			cfg.value("private", true).toBool()
		};

		if(ls.readonly && !includeReadOnly)
			continue;

		if(ls.iconName == "drawpile")
			ls.icon = QIcon(":/icons/drawpile.png");
		else if(!ls.iconName.isEmpty())
			ls.icon = QIcon(iconPath + ls.iconName);

		list << ls;
	}
	cfg.endArray();
	return list;
}

void ListServerModel::loadServers(bool includeReadOnly)
{
	beginResetModel();
	m_servers = listServers(includeReadOnly);
	endResetModel();
}


void ListServerModel::saveServers() const
{
	QSettings cfg;
	cfg.beginWriteArray("listservers", m_servers.size());
	for(int i=0;i<m_servers.size();++i) {
		const ListServer &s = m_servers.at(i);
		cfg.setArrayIndex(i);

		cfg.setValue("name", s.name);
		cfg.setValue("url", s.url);
		cfg.setValue("description", s.description);
		cfg.setValue("icon", s.iconName);
		cfg.setValue("readonly", s.readonly);
		cfg.setValue("public", s.publicListings);
		cfg.setValue("private", s.privateListings);
	}
	cfg.endArray();
}

}
