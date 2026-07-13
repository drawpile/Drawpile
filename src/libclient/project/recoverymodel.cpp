// SPDX-License-Identifier: GPL-3.0-or-later
extern "C" {
#include <dpengine/project.h>
}
#include "libclient/project/metadata.h"
#include "libclient/project/recoverymodel.h"
#include "libclient/utils/pathinfo.h"
#include "libclient/utils/wasmpersistence.h"
#include <QDir>
#include <QHash>
#include <algorithm>

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
		m_status = RecoveryModel::checkRecoveryStatus(m_path, &m_errorMessage);
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

const QString &RecoveryEntry::lastSave() const
{
	loadMetadata();
	return m_lastSave;
}

const QString &RecoveryEntry::lastExport() const
{
	loadMetadata();
	return m_lastExport;
}

const QString &RecoveryEntry::lastSessionTitle() const
{
	loadMetadata();
	return m_lastSessionTitle;
}

long long RecoveryEntry::ownWorkMinutes() const
{
	loadMetadata();
	return m_ownWorkMinutes;
}

QString RecoveryEntry::mostDescriptiveBaseName() const
{
	loadMetadata();

	if(!m_lastSave.isEmpty()) {
		QString lastSaveBaseName = utils::PathInfo(m_lastSave).basename();
		stripExtension(lastSaveBaseName);
		if(!lastSaveBaseName.isEmpty()) {
			return lastSaveBaseName;
		}
	}

	if(!m_lastExport.isEmpty()) {
		QString lastExportBaseName = utils::PathInfo(m_lastExport).basename();
		stripExtension(lastExportBaseName);
		if(!lastExportBaseName.isEmpty()) {
			return lastExportBaseName;
		}
	}

	QString autoRecoveryBaseName = m_baseName;
	stripExtension(autoRecoveryBaseName);
	return autoRecoveryBaseName;
}

void RecoveryEntry::loadMetadata() const
{
	if(!m_metadataLoaded) {
		m_metadataLoaded = true;
		retrieveMetadata(m_path, [this](drawdance::Query &qry) {
			retrieveMetadataString(qry, "last_save", m_lastSave);
			retrieveMetadataString(qry, "last_export", m_lastSave);
			retrieveMetadataString(
				qry, "last_session_title", m_lastSessionTitle);
			retrieveMetadataLongLong(qry, "own_work_minutes", m_ownWorkMinutes);
		});
	}
}

void RecoveryEntry::stripExtension(QString &s)
{
	int i = s.lastIndexOf('.');
	if(i > 0) {
		s.chop(s.length() - i + 1);
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
		{QStringLiteral("*.dppr.resume"), QStringLiteral("*.dppr.meta"),
		 QStringLiteral("*.dppr.thumb")});

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

	DRAWPILE_FS_PERSIST();
}

void RecoveryModel::addOpenedAutosavePath(const QString &path)
{
	openedAutosavePaths.insert(path);
}

void RecoveryModel::removeOpenedAutosavePath(const QString &path)
{
	openedAutosavePaths.remove(path);
}

QFileInfoList RecoveryModel::entryInfoListForBaseDir(const QString &baseDir)
{
	QDir dir(baseDir);
	dir.setNameFilters({QStringLiteral("*.dppr")});
	dir.setFilter(QDir::Files | QDir::NoSymLinks);
	QFileInfoList entryInfos = dir.entryInfoList();

	// Disregard entries that our own process has open.
	for(const QString &path : openedAutosavePaths) {
		entryInfos.removeAll(QFileInfo(path));
	}

	return entryInfos;
}

RecoveryStatus RecoveryModel::checkRecoveryStatus(
	const QString &path, QString *outErrorMessage)
{
	DP_ProjectOpenResult result =
		DP_project_open(path.toUtf8().constData(), DP_PROJECT_OPEN_READ_ONLY);
	if(result.error == 0) {
		DP_project_close(result.project);
		return RecoveryStatus::Available;
	} else {
		if(outErrorMessage) {
			*outErrorMessage = QString::fromUtf8(DP_error());
		}
		if(result.error == DP_PROJECT_OPEN_ERROR_LOCKED) {
			return RecoveryStatus::Locked;
		} else {
			return RecoveryStatus::Error;
		}
	}
}

QFileInfoList RecoveryModel::entryInfoList() const
{
	return entryInfoListForBaseDir(m_baseDir);
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
