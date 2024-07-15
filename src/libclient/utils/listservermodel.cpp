// SPDX-License-Identifier: GPL-3.0-or-later

#include "libclient/settings.h"
#include "libclient/utils/listservermodel.h"
#include "libclient/utils/wasmpersistence.h"
#include "libshared/util/paths.h"
#include <dpcommon/platform_qt.h>
#include <QImage>
#include <QBuffer>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QSslSocket>

namespace sessionlisting {

ListServerModel::ListServerModel(libclient::settings::Settings &settings, bool includeReadOnly, QObject *parent)
	: QAbstractListModel(parent)
	, m_settings(settings)
	, m_includeReadOnly(includeReadOnly)
{
	reload();
}

void ListServerModel::reload()
{
	loadServers(m_settings.listServers(), m_includeReadOnly);
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
		case Qt::ToolTipRole: return QString("%1\n\n%2\n\nURL: %3\nRead-only: %4, public: %5").arg(
			srv.name,
			srv.description,
			srv.url,
			srv.readonly ? QStringLiteral("yes") : QStringLiteral("no"),
			srv.publicListings ? QStringLiteral("yes") : QStringLiteral("no")
			);
		case UrlRole: return srv.url;
		case DescriptionRole: return srv.description;
		case PublicListRole: return srv.publicListings;
		}
	}
	return QVariant();
}

bool ListServerModel::moveServer(int sourceRow, int destinationChild)
{
	if(sourceRow < 0 || sourceRow >= m_servers.size()) {
		return false;
	}

	int destinationRow = qBound(0, destinationChild, m_servers.size()-1);
	if(sourceRow == destinationRow) {
		return false;
	}

	beginMoveRows(
		QModelIndex{}, sourceRow, sourceRow, QModelIndex{},
		destinationRow + (destinationRow > sourceRow ? 1 : 0));
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

bool ListServerModel::addServer(const QString &name, const QString &url, const QString &description, bool readonly, bool pub)
{
	const ListServer lstSrv {
			QIcon(),
			QString(),
			name,
			url,
			description,
			readonly,
			pub,
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

QIcon ListServerModel::setFavicon(const QString &url, const QImage &icon)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	// Make sure the icon is not huge
	QImage scaledIcon;

	if(icon.width() > 128 || icon.height() > 128)
		scaledIcon = icon.scaled(QSize(128, 128), Qt::KeepAspectRatio);
	else
		scaledIcon = icon;

	// Serialize icon in PNG format
	QByteArray data;
	QBuffer buffer(&data);
	buffer.open(DP_QT_WRITE_FLAGS);
	scaledIcon.save(&buffer, "PNG");

	// Generate file name
	QCryptographicHash hash(QCryptographicHash::Sha1);
	hash.addData(data);
	QString iconName = hash.result().toHex() + ".png";
	QIcon iconData{QPixmap::fromImage(scaledIcon)};

	// Find the server
	bool saved = false;
	for(int i=0;i<m_servers.size();++i) {
		ListServer &s = m_servers[i];
		if(s.url != url)
			continue;

		s.iconName = iconName;
		s.icon = iconData;

		if(!saved) {
			saved = true;
			QFile f(utils::paths::writablePath("favicons/", s.iconName));
			if(!f.open(DP_QT_WRITE_FLAGS)) {
				qWarning() << "Unable to open" << f.fileName() << f.errorString();
			} else {
				f.write(data);
				f.close();
			}
		}
	}

	return iconData;
}

QVector<ListServer> ListServerModel::listServers(const QVector<QVariantMap> &cfg, bool includeReadOnly)
{
	QVector<ListServer> list;

	const QString iconPath = utils::paths::writablePath("favicons/");
	for(const auto &item : cfg) {
		if(item.isEmpty()) {
			continue; // Old Drawpile versions leave voids sometimes.
		}

		ListServer ls {
			QIcon(),
			item.value("icon").toString(),
			item.value("name").toString(),
			item.value("url").toString(),
			item.value("description").toString(),
			item.value("readonly").toBool(),
			item.value("public", true).toBool(),
		};

		if(ls.readonly && !includeReadOnly)
			continue;

		if(ls.iconName == "drawpile")
			ls.icon = QIcon(":/icons/drawpile.png");
		else if(!ls.iconName.isEmpty())
			ls.icon = QIcon(iconPath + ls.iconName);

		list << ls;
	}
	return list;
}

void ListServerModel::loadServers(const QVector<QVariantMap> &cfg, bool includeReadOnly)
{
	beginResetModel();
	m_servers = listServers(cfg, includeReadOnly);
	endResetModel();
}

QVector<QVariantMap> ListServerModel::saveServers() const
{
	QVector<QVariantMap> cfg;
	for(int i=0;i<m_servers.size();++i) {
		const ListServer &s = m_servers.at(i);
		cfg.append({
			{ "name", s.name },
			{ "url", s.url },
			{ "description", s.description },
			{ "icon", s.iconName },
			{ "readonly", s.readonly },
			{ "public", s.publicListings },
		});
	}
	return cfg;
}

void ListServerModel::revert()
{
	loadServers(m_settings.listServers(), m_includeReadOnly);
}

bool ListServerModel::submit()
{
	m_settings.setListServers(saveServers());
	return true;
}

}
