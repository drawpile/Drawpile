// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/recents.h"
#include "cmake-config/config.h"
#include "desktop/main.h"
#include "desktop/settings.h"
#include "libclient/utils/statedatabase.h"
#include "libclient/utils/wasmpersistence.h"
#include "libshared/util/paths.h"
#include <QFileInfo>
#include <QMenu>
#include <QPair>
#include <QRegularExpression>
#include <QStringList>

namespace utils {

QString Recents::Host::toString() const
{
	return toStringFrom(host, port);
}

QString Recents::Host::toStringFrom(const QString &host, int port)
{
	if(port <= 0 || port == cmake_config::proto::port()) {
		return host.trimmed();
	} else {
		return QStringLiteral("%1:%2").arg(host.trimmed()).arg(port);
	}
}

Recents::Recents(StateDatabase &state)
	: QObject{&state}
	, m_state{state}
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	createTables();
#ifndef __EMSCRIPTEN__
	migrateFilesFromSettings();
	migrateHostsFromSettings();
#endif
}

#ifndef __EMSCRIPTEN__

QVector<Recents::File> Recents::getFiles() const
{
	drawdance::Query qry = m_state.query();
	QVector<Recents::File> files;
	if(qry.exec("select recent_file_id, path "
				"from recent_files order by weight")) {
		while(qry.next()) {
			files.append({qry.columnInt64(0), qry.columnText16(1)});
		}
	}
	return files;
}

static int recentsFileCountWith(drawdance::Query &qry)
{
	if(qry.exec("select count(*) from recent_files") && qry.next()) {
		return qry.columnInt(0);
	} else {
		return -1;
	}
}

int Recents::fileCount() const
{
	drawdance::Query qry = m_state.query();
	return recentsFileCountWith(qry);
}

void Recents::addFile(const QString &path)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	bool ok = m_state.tx([&path](drawdance::Query &qry) {
		if(!qry.exec("select recent_file_id, path "
					 "from recent_files order by weight")) {
			return false;
		}

		bool found = false;
		QVector<long long> ids;
		while(qry.next()) {
			long long id = qry.columnInt64(0);
			QString existingPath = qry.columnText16(1, true);
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

		if(!qry.prepare(
			   "update recent_files set weight = ? where recent_file_id = ?")) {
			return false;
		}

		int offset = found ? 0 : 1;
		int count = ids.size();
		for(int i = 0; i < count; ++i) {
			if(!qry.bind(0, i + offset) || !qry.bind(1, ids[i]) ||
			   !qry.execPrepared()) {
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
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
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

#endif

QVector<Recents::Host> Recents::getHosts() const
{
	drawdance::Query qry = m_state.query();
	QVector<Recents::Host> hosts;
	if(qry.exec("select recent_host_id, host, port, flags\n"
				"from recent_hosts order by weight")) {
		while(qry.next()) {
			int flags = qry.columnInt(3);
			hosts.append(
				{qry.columnInt64(0), qry.columnText16(1), qry.columnInt(2),
				 (flags & FLAG_JOINED) != 0, (flags & FLAG_HOSTED) != 0});
		}
	}
	return hosts;
}

QString Recents::getMostRecentHostAddress() const
{
	drawdance::Query qry = m_state.query();
	if(qry.exec(
		   "select host, port from recent_hosts where flags & ? "
		   "order by weight limit 1",
		   {FLAG_HOSTED}) &&
	   qry.next()) {
		return Host::toStringFrom(qry.columnText16(0), qry.columnInt(1));
	}
	return QString();
}

static int recentHostCountWith(drawdance::Query &qry)
{
	if(qry.exec("select count(*) from recent_hosts") && qry.next()) {
		return qry.columnInt(0);
	} else {
		return -1;
	}
}

int Recents::hostCount() const
{
	drawdance::Query qry = m_state.query();
	return recentHostCountWith(qry);
}

void Recents::addHost(
	const QString &host, int port, bool joined, bool hosted, bool webSocket)
{
	struct AddHost {
		long long id;
		QString host;
		int port;
		int flags;
	};
	QString effectiveHost =
		webSocket ? QStringLiteral("%1?w").arg(
						QString::fromUtf8(QUrl::toPercentEncoding(host)))
				  : host;
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	bool ok = m_state.tx([&effectiveHost, port, joined,
						  hosted](drawdance::Query &qry) {
		if(!qry.exec("select recent_host_id, host, port, flags "
					 "from recent_hosts order by weight")) {
			return false;
		}

		bool found = false;
		QVector<AddHost> ahs;
		while(qry.next()) {
			AddHost ah = {
				qry.columnInt(0),
				qry.columnText16(1),
				qry.columnInt(2),
				qry.columnInt(3),
			};
			if(ah.host.compare(effectiveHost, Qt::CaseInsensitive) == 0 &&
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
				   "insert into recent_hosts (host, port, flags, weight) "
				   "values (?, ?, ?, ?)",
				   {effectiveHost, port, packHostFlags(joined, hosted), 0})) {
				return false;
			}
		}

		if(!qry.prepare("update recent_hosts set flags = ?, weight = ? "
						"where recent_host_id = ?")) {
			return false;
		}

		int offset = found ? 0 : 1;
		int count = ahs.size();
		for(int i = 0; i < count; ++i) {
			const AddHost &ah = ahs[i];
			if(!qry.bind(0, ah.flags) || !qry.bind(1, i + offset) ||
			   !qry.bind(2, ah.id) || !qry.execPrepared()) {
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
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	if(removeById("delete from recent_hosts where recent_host_id = ?", id)) {
		emit recentHostsChanged();
		return true;
	} else {
		return false;
	}
}

void Recents::createTables()
{
	drawdance::Query qry = m_state.query();
#ifndef __EMSCRIPTEN__
	qry.exec("create table if not exists recent_files ("
			 "recent_file_id integer primary key not null,"
			 "path text not null,"
			 "weight integer not null)");
#endif
	qry.exec("create table if not exists recent_hosts ("
			 "recent_host_id integer primary key not null,"
			 "host text not null,"
			 "port integer not null,"
			 "flags integer not null,"
			 "weight integer not null)");
}

#ifndef __EMSCRIPTEN__

void Recents::migrateFilesFromSettings()
{
	m_state.tx([this](drawdance::Query &qry) {
		QString key = QStringLiteral("recents/filesmigratedfromsettings");
		if(!m_state.getBoolWith(qry, key, false)) {
			m_state.putWith(qry, key, true);
			if(recentsFileCountWith(qry) != 0) {
				return true;
			}

			if(!qry.prepare("insert into recent_files (path, weight) "
							"values (?, ?)")) {
				return false;
			}

			QStringList settingsRecentFiles = dpApp().settings().recentFiles();
			int count = qMin(MAX_RECENT_FILES, settingsRecentFiles.size());
			for(int i = 0; i < count; ++i) {
				if(!qry.bind(0, settingsRecentFiles[i]) || !qry.bind(1, i) ||
				   !qry.execPrepared()) {
					return false;
				}
			}
		}
		return true;
	});
}

void Recents::migrateHostsFromSettings()
{
	m_state.tx([this](drawdance::Query &qry) {
		QString key = QStringLiteral("recents/hostsmigratedfromsettings");
		if(!m_state.getBoolWith(qry, key, false)) {
			m_state.putWith(qry, key, true);
			if(recentHostCountWith(qry) != 0) {
				return true;
			}

			if(!qry.prepare(
				   "insert into recent_hosts (host, port, flags, weight) "
				   "values (?, ?, ?, ?)")) {
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
				if(!qry.bind(0, rh.host) || !qry.bind(1, rh.port) ||
				   !qry.bind(2, packHostFlags(rh.joined, rh.hosted)) ||
				   !qry.bind(3, i) || !qry.execPrepared()) {
					return false;
				}
			}
		}
		return true;
	});
}

#endif

bool Recents::removeById(const QString &sql, int id)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	drawdance::Query qry = m_state.query();
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

#ifndef __EMSCRIPTEN__

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
	drawdance::Query qry = m_state.query();
	QStringList paths;
	if(qry.exec(
		   "select path from recent_files order by weight limit ?",
		   {MAX_MENU_FILES})) {
		while(qry.next()) {
			paths.append(qry.columnText16(0));
		}
	}
	return paths;
}

#endif

int Recents::packHostFlags(bool joined, bool hosted)
{
	return (joined ? FLAG_JOINED : 0) | (hosted ? FLAG_HOSTED : 0);
}

}
