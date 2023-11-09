// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/recents.h"
#include "cmake-config/config.h"
#include "desktop/main.h"
#include "libclient/utils/statedatabase.h"
#include "libshared/util/paths.h"
#include <QFileInfo>
#include <QMenu>
#include <QPair>
#include <QStringList>

namespace utils {

QString Recents::Host::toString() const
{
	if(port == cmake_config::proto::port()) {
		return host;
	} else {
		return QStringLiteral("%1:%2").arg(host).arg(port);
	}
}

Recents::Recents(StateDatabase &state)
	: QObject{&state}
	, m_state{state}
{
	createTables();
	migrateFilesFromSettings();
	migrateHostsFromSettings();
}

QVector<Recents::File> Recents::getFiles() const
{
	StateDatabase::Query qry = m_state.query();
	QVector<Recents::File> files;
	if(qry.exec("select recent_file_id, path\n"
				"from recent_files order by weight")) {
		while(qry.next()) {
			files.append({qry.value(0).toLongLong(), qry.value(1).toString()});
		}
	}
	return files;
}

int Recents::fileCount() const
{
	StateDatabase::Query qry = m_state.query();
	if(qry.exec("select count(*) from recent_files") && qry.next()) {
		return qry.value(0).toInt();
	} else {
		return -1;
	}
}

void Recents::addFile(const QString &path)
{
	bool ok = m_state.tx([&path](StateDatabase::Query &qry) {
		if(!qry.exec("select recent_file_id, path\n"
					 "from recent_files order by weight")) {
			return false;
		}

		bool found = false;
		QVector<long long> ids;
		while(qry.next()) {
			long long id = qry.value(0).toInt();
			QString existingPath = qry.value(1).toString();
			if(existingPath == path) {
				found = true;
				ids.prepend(id);
			} else {
				ids.append(id);
			}
		}

		if(!found) {
			if(!qry.exec(
				   "insert into recent_files (path, weight) values (?, ?)",
				   {path, 0})) {
				return false;
			}
		}

		QString updateSql = QStringLiteral(
			"update recent_files set weight = ? where recent_file_id = ?");
		if(!qry.prepare(updateSql)) {
			return false;
		}

		int offset = found ? 0 : 1;
		int count = ids.size();
		for(int i = 0; i < count; ++i) {
			qry.bindValue(0, i + offset);
			qry.bindValue(1, ids[i]);
			if(!qry.execPrepared()) {
				return false;
			};
		}

		return qry.exec(
			"delete from recent_files where weight >= ?", {MAX_RECENT_FILES});
	});

	if(ok) {
		emit recentFilesChanged();
	}
}

bool Recents::removeFileById(long long id)
{
	if(removeById("delete from recent_files where recent_file_id = ?", id)) {
		emit recentFilesChanged();
		return true;
	} else {
		return false;
	}
}

void Recents::bindFileMenu(QMenu *menu)
{
	connect(this, &Recents::recentFilesChanged, menu, [=] {
		updateFileMenu(menu);
	});
	updateFileMenu(menu);
}

QVector<Recents::Host> Recents::getHosts() const
{
	StateDatabase::Query qry = m_state.query();
	QVector<Recents::Host> hosts;
	if(qry.exec("select recent_host_id, host, port, flags\n"
				"from recent_hosts order by weight")) {
		while(qry.next()) {
			int flags = qry.value(3).toInt();
			hosts.append(
				{qry.value(0).toLongLong(), qry.value(1).toString(),
				 qry.value(2).toInt(), (flags & FLAG_JOINED) != 0,
				 (flags & FLAG_HOSTED) != 0});
		}
	}
	return hosts;
}

int Recents::hostCount() const
{
	StateDatabase::Query qry = m_state.query();
	if(qry.exec("select count(*) from recent_hosts") && qry.next()) {
		return qry.value(0).toInt();
	} else {
		return -1;
	}
}

void Recents::addHost(const QString &host, int port, bool joined, bool hosted)
{
	struct AddHost {
		long long id;
		QString host;
		int port;
		int flags;
	};
	bool ok = m_state.tx([&host, port, joined,
						  hosted](StateDatabase::Query &qry) {
		if(!qry.exec("select recent_host_id, host, port, flags\n"
					 "from recent_hosts order by weight")) {
			return false;
		}

		bool found = false;
		QVector<AddHost> ahs;
		while(qry.next()) {
			AddHost ah = {
				qry.value(0).toInt(),
				qry.value(1).toString(),
				qry.value(2).toInt(),
				qry.value(3).toInt(),
			};
			if(ah.host.compare(host, Qt::CaseInsensitive) == 0 &&
			   ah.port == port) {
				found = true;
				ah.flags |= packHostFlags(joined, hosted);
				ahs.prepend(ah);
			} else {
				ahs.append(ah);
			}
		}

		if(!found) {
			if(!qry.exec(
				   "insert into recent_hosts (host, port, flags, weight)\n"
				   "values (?, ?, ?, ?)",
				   {host, port, packHostFlags(joined, hosted), 0})) {
				return false;
			}
		}

		QString updateSql =
			QStringLiteral("update recent_hosts set flags = ?, weight = ?\n"
						   "where recent_host_id = ?");
		if(!qry.prepare(updateSql)) {
			return false;
		}

		int offset = found ? 0 : 1;
		int count = ahs.size();
		for(int i = 0; i < count; ++i) {
			const AddHost &ah = ahs[i];
			qry.bindValue(0, ah.flags);
			qry.bindValue(1, i + offset);
			qry.bindValue(2, ah.id);
			if(!qry.execPrepared()) {
				return false;
			};
		}

		return qry.exec(
			"delete from recent_hosts where weight >= ?", {MAX_RECENT_FILES});
	});

	if(ok) {
		emit recentHostsChanged();
	}
}

bool Recents::removeHostById(long long id)
{
	if(removeById("delete from recent_hosts where recent_host_id = ?", id)) {
		emit recentHostsChanged();
		return true;
	} else {
		return false;
	}
}

void Recents::createTables()
{
	StateDatabase::Query qry = m_state.query();
	qry.exec("create table if not exists recent_files (\n"
			 "	recent_file_id integer primary key not null,\n"
			 "	path text not null,\n"
			 "	weight integer not null)");
	qry.exec("create table if not exists recent_hosts (\n"
			 "	recent_host_id integer primary key not null,\n"
			 "	host text not null,\n"
			 "	port integer not null,\n"
			 "	flags integer not null,\n"
			 "	weight integer not null)");
}

void Recents::migrateFilesFromSettings()
{
	m_state.tx([this](StateDatabase::Query &qry) {
		QString key = QStringLiteral("recents/filesmigratedfromsettings");
		if(!qry.get(key).toBool()) {
			qry.put(key, true);
			if(fileCount() != 0) {
				return true;
			}

			QString sql =
				QStringLiteral("insert into recent_files (path, weight)\n"
							   "values (?, ?)");
			if(!qry.prepare(sql)) {
				return false;
			}

			QStringList settingsRecentFiles = dpApp().settings().recentFiles();
			int count = qMin(MAX_RECENT_FILES, settingsRecentFiles.size());
			for(int i = 0; i < count; ++i) {
				qry.bindValue(0, settingsRecentFiles[i]);
				qry.bindValue(1, i);
				if(!qry.execPrepared()) {
					return false;
				}
			}
		}
		return true;
	});
}

void Recents::migrateHostsFromSettings()
{
	m_state.tx([this](StateDatabase::Query &qry) {
		QString key = QStringLiteral("recents/hostsmigratedfromsettings");
		if(!qry.get(key).toBool()) {
			qry.put(key, true);
			if(hostCount() != 0) {
				return true;
			}

			QString sql = QStringLiteral(
				"insert into recent_hosts (host, port, flags, weight)\n"
				"values (?, ?, ?, ?)");
			if(!qry.prepare(sql)) {
				return false;
			}

			QVector<Recents::Host> rhs;
			desktop::settings::Settings &settings = dpApp().settings();
			for(const QString &joinHost : settings.recentHosts()) {
				collectSettingsHost(rhs, joinHost, true);
			}
			for(const QString &hostHost : settings.recentRemoteHosts()) {
				collectSettingsHost(rhs, hostHost, false);
			}

			int count = qMin(MAX_RECENT_HOSTS, rhs.size());
			for(int i = 0; i < count; ++i) {
				const Recents::Host &rh = rhs[i];
				qry.bindValue(0, rh.host);
				qry.bindValue(1, rh.port);
				qry.bindValue(2, packHostFlags(rh.joined, rh.hosted));
				qry.bindValue(3, i);
				if(!qry.execPrepared()) {
					return false;
				}
			}
		}
		return true;
	});
}

bool Recents::removeById(const QString &sql, int id)
{
	StateDatabase::Query qry = m_state.query();
	return qry.exec(sql, {id}) && qry.numRowsAffected() > 0;
}

void Recents::collectSettingsHost(
	QVector<Recents::Host> &rhs, const QString &input, bool join)
{
	static QRegularExpression re{"\\A([^:]+)(?::([0-9]+))?\\z"};
	QRegularExpressionMatch match = re.match(input);
	if(match.hasMatch()) {
		QString host = match.captured(1);
		int port = match.captured(2).toInt();
		if(port < 1 || port > 65535) {
			port = cmake_config::proto::port();
		}

		for(Recents::Host &rh : rhs) {
			if(rh.host.compare(host, Qt::CaseInsensitive) == 0 &&
			   rh.port == port) {
				if(join) {
					rh.joined = true;
				} else {
					rh.hosted = true;
				}
				return;
			}
		}

		rhs.append({0, host, port, join, !join});
	} else {
		qWarning("Can't parse host '%s'", qUtf8Printable(input));
	}
}


void Recents::updateFileMenu(QMenu *menu) const
{
	QStringList paths = getFileMenuPaths();
	const QList<QAction *> actions = menu->actions();
	for(QAction *act : actions) {
		menu->removeAction(act);
		act->deleteLater();
	}

	menu->setEnabled(!paths.isEmpty());
	int count = paths.size();
	for(int i = 0; i < count; ++i) {
		QString path = paths[i];
		QAction *a = menu->addAction(QString(i < 10 ? "&%1. %2" : "%1. %2")
										 .arg(i + 1)
										 .arg(paths::extractBasename(path)));
		QString absoluteFilePath = QFileInfo{path}.absoluteFilePath();
		a->setStatusTip(absoluteFilePath);
		a->setProperty("filepath", absoluteFilePath);
	}

	menu->addSeparator();
	menu->addAction(QStringLiteral("&0. %1").arg(tr("More…")));
}

QStringList Recents::getFileMenuPaths() const
{
	StateDatabase::Query qry = m_state.query();
	QStringList paths;
	if(qry.exec(
		   "select path from recent_files order by weight limit ?",
		   {MAX_MENU_FILES})) {
		while(qry.next()) {
			paths.append(qry.value(0).toString());
		}
	}
	return paths;
}

int Recents::packHostFlags(bool joined, bool hosted)
{
	return (joined ? FLAG_JOINED : 0) | (hosted ? FLAG_HOSTED : 0);
}

}
