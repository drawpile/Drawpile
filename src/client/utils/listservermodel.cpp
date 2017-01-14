/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2015-2017 Calle Laakkonen

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

#include <QDebug>
#include <QImage>
#include <QBuffer>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QFile>
#include <QDir>
#include <QSettings>
#include <QSslSocket>

namespace sessionlisting {

ListServerModel::ListServerModel(bool showlocal, QObject *parent)
	: QAbstractListModel(parent), m_showlocal(showlocal)
{
	loadServers();
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
		case Qt::UserRole: return srv.url;
		case Qt::UserRole+1: return srv.description;
		}
	}
	return QVariant();
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

void ListServerModel::addServer(const QString &name, const QString &url, const QString &description)
{
	beginInsertRows(QModelIndex(), m_servers.size(), m_servers.size());
	m_servers << ListServer {
		QIcon(),
		QString(),
		name,
		url,
		description
	};
	endInsertRows();
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
		QString path = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/favicons/";

		QDir(path).mkpath(".");
		QFile f(path + s.iconName);
		if(!f.open(QIODevice::WriteOnly)) {
			qWarning() << "Unable to open" << path + s.iconName << f.errorString();
		} else {
			f.write(data);
			f.close();
		}
	}
}

void ListServerModel::loadServers()
{
	beginResetModel();
	m_servers.clear();

	QSettings cfg;
	const QString iconPath = QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/favicons/";
	const int count = cfg.beginReadArray("listservers");
	for(int i=0;i<count;++i) {
		cfg.setArrayIndex(i);
		ListServer ls {
			QIcon(),
			cfg.value("icon").toString(),
			cfg.value("name").toString(),
			cfg.value("url").toString(),
			cfg.value("description").toString()
		};

		if(ls.iconName == "drawpile")
			ls.icon = QIcon("builtin:drawpile.png");
		else if(!ls.iconName.isEmpty())
			ls.icon = QIcon(iconPath + ls.iconName);

		m_servers << ls;
	}
	cfg.endArray();

	// Add the default drawpile.net server if there is nothing else
	if(m_servers.isEmpty()) {
		m_servers << ListServer {
			QIcon("builtin:drawpile.png"),
			QStringLiteral("drawpile"),
			QStringLiteral("drawpile.net"),
			QStringLiteral("http://drawpile.net/api/listing/"),
			QStringLiteral("This is the default public listing server.\n"
			"Note that as this server is open to all, please do not share any images that would "
			"not suitable for everyone.")
		};
	}

	// Use HTTPS for drawpile.net listing if possible. It would be better to
	// just always use HTTPS, but SSL support is not always available (on Windows,
	// since OpenSSL is not part of the base system.)
	if(QSslSocket::supportsSsl()) {
		for(ListServer &ls : m_servers) {
			if(ls.url == QStringLiteral("http://drawpile.net/api/listing/"))
				ls.url = QStringLiteral("https://drawpile.net/api/listing/");
		}
	}

#ifdef HAVE_DNSSD
	// Add an entry for local server discovery
	if(m_showlocal) {
		m_servers.prepend(ListServer {
			QIcon(),
			QString(),
			tr("Nearby"),
			QStringLiteral("local"),
			QString()
		});
	}
#endif

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
	}
	cfg.endArray();
}

}
