// SPDX-License-Identifier: GPL-3.0-or-later
#include "libshared/util/database.h"
#include "libshared/util/paths.h"
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcDpDatabase)

namespace utils {
namespace db {
namespace {

void copyFrom(
	const QString &databasePath, const QString &sourceFileName,
	const QFileInfo &fileInfo)
{
	if(fileInfo.exists()) {
		qCDebug(
			lcDpDatabase, "Database file '%s' already exists",
			qUtf8Printable(databasePath));
	} else {
		QString initialPath = utils::paths::locateDataFile(sourceFileName);
		if(initialPath.isEmpty()) {
			qCWarning(
				lcDpDatabase, "No database found at '%s'",
				qUtf8Printable(initialPath));
		} else {
			QFile initialFile{initialPath};
			if(initialFile.copy(databasePath)) {
				qDebug(
					"Created database '%s' from '%s'",
					qUtf8Printable(databasePath), qUtf8Printable(initialPath));
			} else {
				qWarning(
					"Could not create database '%s' from '%s': %s",
					qUtf8Printable(databasePath), qUtf8Printable(initialPath),
					qUtf8Printable(initialFile.errorString()));
			}
		}
	}
}

void fixFilePermissions(const QString &databasePath, const QFileInfo &fileInfo)
{
	QFile databaseFile{databasePath};
	if(fileInfo.exists()) {
		QFlags<QFileDevice::Permission> readWritePermissions =
			QFileDevice::ReadUser | QFileDevice::WriteUser;
		bool hasReadWritePermission =
			(databaseFile.permissions() & readWritePermissions) ==
			readWritePermissions;
		if(hasReadWritePermission) {
			qCDebug(
				lcDpDatabase, "Permissions of '%s' are already read-write",
				qUtf8Printable(databasePath));
		} else {
			if(databaseFile.setPermissions(readWritePermissions)) {
				qCDebug(
					lcDpDatabase, "Set permissions of '%s' to read-write",
					qUtf8Printable(databasePath));
			} else {
				qCWarning(
					lcDpDatabase,
					"Could not set permissions of '%s' to read-write: %s",
					qUtf8Printable(databasePath),
					qUtf8Printable(databaseFile.errorString()));
			}
		}
	} else {
		qCDebug(
			lcDpDatabase, "Database file '%s' doesn't exist yet",
			qUtf8Printable(databasePath));
	}
}

}

bool open(
	drawdance::Database &db, const QString &humaneName, const QString &fileName,
	const QString &sourceFileName)
{
	QString databasePath = utils::paths::writablePath(fileName);
	QFileInfo fileInfo(databasePath);
	if(!sourceFileName.isEmpty()) {
		copyFrom(databasePath, sourceFileName, fileInfo);
	}
	fixFilePermissions(databasePath, fileInfo);
	return db.open(databasePath, humaneName);
}

}
}
