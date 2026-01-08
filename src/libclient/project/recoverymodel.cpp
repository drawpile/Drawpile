// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/project.h>
}
#include "libclient/project/recoverymodel.h"
#include <QDir>
#include <QHash>
#include <algorithm>
#include <libshared/util/database.h>

namespace project {

QSet<QString> RecoveryModel::openedAutosavePaths;


RecoveryEntry::RecoveryEntry(const QFileInfo &fileInfo)
	: m_fileInfo(fileInfo)
	, m_path(fileInfo.filePath())
	, m_baseName(fileInfo.fileName())
{
}

RecoveryStatus RecoveryEntry::status() const
{
	if(!m_statusLoaded) {
		m_statusLoaded = true;
		QString path = m_path;
		DP_ProjectOpenResult result = DP_project_open(
			path.toUtf8().constData(), DP_PROJECT_OPEN_READ_ONLY);
		if(result.error == 0) {
			DP_project_close(result.project);
			m_status = RecoveryStatus::Available;
		} else {
			m_errorMessage = QString::fromUtf8(DP_error());
			if(result.error == DP_PROJECT_OPEN_ERROR_LOCKED) {
				m_status = RecoveryStatus::Locked;
			} else {
				m_status = RecoveryStatus::Error;
			}
		}
	}
	return m_status;
}

qint64 RecoveryEntry::fileSize() const
{
	if(!m_fileSizeLoaded) {
		m_fileSizeLoaded = true;
		m_fileSize = m_fileInfo.size();
	}
	return m_fileSize;
}

const QDateTime &RecoveryEntry::mtime() const
{
	if(!m_mtimeLoaded) {
		m_mtimeLoaded = true;
		m_mtime = m_fileInfo.lastModified();
	}
	return m_mtime;
}

const QPixmap &RecoveryEntry::thumbnail() const
{
	if(!m_thumbnailLoaded) {
		m_thumbnailLoaded = true;
		QString path = m_fileInfo.filePath() + QStringLiteral(".thumb");
		if(!m_thumbnail.load(path)) {
			qWarning(
				"Failed to load recovery entry thumbnail '%s'",
				qUtf8Printable(path));
		}
	}
	return m_thumbnail;
}

long long RecoveryEntry::ownWorkMinutes() const
{
	loadMetadata();
	return m_ownWorkMinutes;
}

void RecoveryEntry::loadMetadata() const
{
	if(!m_metadataLoaded) {
		m_metadataLoaded = true;

		drawdance::Database db;
		bool openOk = db.open(
			m_path + QStringLiteral(".meta"),
			QStringLiteral("autosave metadata"));
		if(openOk) {
			drawdance::Query qry = db.query();

			if(qry.prepare("select value from metadata where name = ?")) {
				if(qry.bind(0, "own_work_minutes") && qry.execPrepared() &&
				   qry.next()) {
					m_ownWorkMinutes = qry.columnInt64(0);
				}
			}
		}
	}
}


RecoveryModel::RecoveryModel(const QString &baseDir, QObject *parent)
	: QAbstractItemModel(parent)
	, m_baseDir(baseDir)
{
}

QModelIndex
RecoveryModel::index(int row, int column, const QModelIndex &parent) const
{
	if(isValidIndex(row, column, parent)) {
		return createIndex(row, column);
	} else {
		return QModelIndex();
	}
}

QModelIndex RecoveryModel::parent(const QModelIndex &child) const
{
	Q_UNUSED(child);
	return QModelIndex();
}

int RecoveryModel::rowCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return 0;
	} else {
		return int(m_entries.size());
	}
}

int RecoveryModel::columnCount(const QModelIndex &parent) const
{
	if(parent.isValid()) {
		return 0;
	} else {
		return 1;
	}
}

bool RecoveryModel::hasChildren(const QModelIndex &parent) const
{
	Q_UNUSED(parent);
	return false;
}

QVariant RecoveryModel::data(const QModelIndex &index, int role) const
{
	if(!index.isValid()) {
		return QVariant();
	}

	int row = index.row();
	if(!isValidIndex(row, index.column(), index.parent())) {
		return QVariant();
	}

	switch(role) {
	case Qt::DisplayRole:
		return m_entries[row].baseName();
	case Qt::DecorationRole:
		return m_entries[row].thumbnail();
	default:
		break;
	}

	return QVariant();
}

bool RecoveryModel::checkPotentialEntries()
{
	load();
	for(const RecoveryEntry &entry : m_entries) {
		if(entry.status() != RecoveryStatus::Locked) {
			return true;
		}
	}
	return false;
}

void RecoveryModel::load()
{
	if(!m_loaded) {
		reload();
	}
}

void RecoveryModel::reload()
{
	QVector<RecoveryEntry> entries;
	for(const QFileInfo &fi : entryInfoList()) {
		entries.append(RecoveryEntry(fi));
	}

	std::sort(entries.begin(), entries.end(), &RecoveryModel::entryLessThan);

	beginResetModel();
	m_loaded = true;
	m_entries = std::move(entries);
	endResetModel();

	Q_EMIT loaded();
}

void RecoveryModel::removeOrphanedFiles()
{
	QDir dir(m_baseDir);
	dir.setFilter(QDir::Files | QDir::NoSymLinks);
	dir.setNameFilters(
		{QStringLiteral("*.dppr.meta"), QStringLiteral("*.dppr.thumb")});

	QHash<QString, bool> basePathsExist;
	for(const QFileInfo &fileInfo : dir.entryInfoList()) {
		QString basePath =
			fileInfo.path() + QDir::separator() + fileInfo.completeBaseName();

		bool exists;
		QHash<QString, bool>::const_iterator it =
			basePathsExist.constFind(basePath);
		if(it == basePathsExist.constEnd()) {
			exists = QFileInfo::exists(basePath);
			basePathsExist.insert(basePath, exists);
		} else {
			exists = *it;
		}

		if(!exists) {
			QFile file(fileInfo.filePath());
			if(!file.remove()) {
				qWarning(
					"Failed to remove orphaned file '%s': %s",
					qUtf8Printable(file.fileName()),
					qUtf8Printable(file.errorString()));
			}
		}
	}
}

void RecoveryModel::addOpenedAutosavePath(const QString &path)
{
	openedAutosavePaths.insert(path);
}

void RecoveryModel::removeOpenedAutosavePath(const QString &path)
{
	openedAutosavePaths.remove(path);
}

QFileInfoList RecoveryModel::entryInfoList() const
{
	QDir dir(m_baseDir);
	dir.setNameFilters({QStringLiteral("*.dppr")});
	dir.setFilter(QDir::Files | QDir::NoSymLinks);
	QFileInfoList entryInfos = dir.entryInfoList();

	// Disregard entries that our own process has open.
	for(const QString &path : openedAutosavePaths) {
		entryInfos.removeAll(QFileInfo(path));
	}

	return entryInfos;
}

bool RecoveryModel::entryLessThan(
	const RecoveryEntry &a, const RecoveryEntry &b)
{
	const QDateTime &aMtime = a.mtime();
	const QDateTime &bMtime = b.mtime();
	if(aMtime == bMtime) {
		return a.baseName().compare(b.baseName(), Qt::CaseInsensitive) < 0;
	} else {
		return aMtime < bMtime;
	}
}

}
