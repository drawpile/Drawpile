// SPDX-License-Identifier: GPL-3.0-or-later

#include "desktop/utils/recentfiles.h"
#include "desktop/main.h"
#include "libclient/utils/statedatabase.h"
#include "libshared/util/paths.h"
#include <QFileInfo>
#include <QMenu>
#include <QPair>
#include <QStringList>

namespace utils {

RecentFiles::RecentFiles(StateDatabase &state)
	: QObject{&state}
	, m_state{state}
{
	createTables();
	migrateFromSettings();
}

QVector<RecentFiles::File> RecentFiles::getFiles() const
{
	QSqlQuery qry = m_state.query();
	QVector<RecentFiles::File> files;
	if(m_state.exec(
		   qry, "select recent_file_id, path\n"
				"from recent_files order by weight")) {
		while(qry.next()) {
			files.append({qry.value(0).toLongLong(), qry.value(1).toString()});
		}
	}
	return files;
}

int RecentFiles::fileCount() const
{
	QSqlQuery qry = m_state.query();
	if(m_state.exec(qry, "select count(*) from recent_files") && qry.next()) {
		return qry.value(0).toInt();
	} else {
		return -1;
	}
}

void RecentFiles::addFile(const QString &path)
{
	bool ok = m_state.tx([&](QSqlQuery &qry) {
		if(!m_state.exec(
			   qry, "select recent_file_id, path\n"
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
			if(!m_state.exec(
				   qry, "insert into recent_files (path, weight) values (?, ?)",
				   {path, 0})) {
				return false;
			}
		}

		QString updateSql = QStringLiteral(
			"update recent_files set weight = ? where recent_file_id = ?");
		if(!m_state.prepare(qry, updateSql)) {
			return false;
		}

		int offset = found ? 0 : 1;
		int count = ids.size();
		for(int i = 0; i < count; ++i) {
			qry.bindValue(0, i + offset);
			qry.bindValue(1, ids[i]);
			if(!m_state.execPrepared(qry, updateSql)) {
				return false;
			};
		}

		return m_state.exec(
			qry, "delete from recent_files where weight >= ?",
			{MAX_RECENT_FILES});
	});

	if(ok) {
		emit recentFilesChanged();
	}
}

bool RecentFiles::removeFileById(long long id)
{
	QSqlQuery qry = m_state.query();
	if(m_state.exec(
		   qry, "delete from recent_files where recent_file_id = ?", {id}) &&
	   qry.numRowsAffected() > 0) {
		emit recentFilesChanged();
		return true;
	} else {
		return false;
	}
}

void RecentFiles::bindMenu(QMenu *menu)
{
	connect(this, &RecentFiles::recentFilesChanged, menu, [=] {
		updateMenu(menu);
	});
	updateMenu(menu);
}

void RecentFiles::createTables()
{
	QSqlQuery qry = m_state.query();
	m_state.exec(
		qry, "create table if not exists recent_files (\n"
			 "	recent_file_id integer primary key not null,\n"
			 "	path text not null,\n"
			 "	weight integer not null)");
}

void RecentFiles::migrateFromSettings()
{
	m_state.tx([this](QSqlQuery &qry) {
		QString key = QStringLiteral("recentfiles/migratedfromsettings");
		if(!m_state.getWith(qry, key).toBool()) {
			m_state.putWith(qry, key, true);
			if(fileCount() != 0) {
				return true;
			}

			QString sql =
				QStringLiteral("insert into recent_files (path, weight)\n"
							   "values (?, ?)");
			if(!m_state.prepare(qry, sql)) {
				return false;
			}

			QStringList settingsRecentFiles = dpApp().settings().recentFiles();
			int count = qMin(MAX_RECENT_FILES, settingsRecentFiles.size());
			for(int i = 0; i < count; ++i) {
				qry.bindValue(0, settingsRecentFiles[i]);
				qry.bindValue(1, i);
				if(!m_state.execPrepared(qry, sql)) {
					return false;
				}
			}
		}
		return true;
	});
}

void RecentFiles::updateMenu(QMenu *menu) const
{
	QStringList paths = getMenuPaths();
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

QStringList RecentFiles::getMenuPaths() const
{
	QSqlQuery qry = m_state.query();
	QStringList paths;
	if(m_state.exec(
		   qry, "select path from recent_files order by weight limit ?",
		   {MAX_MENU_FILES})) {
		while(qry.next()) {
			paths.append(qry.value(0).toString());
		}
	}
	return paths;
}

}
