// SPDX-License-Identifier: GPL-3.0-or-later

#include "libshared/util/database.h"
#include "libshared/util/paths.h"
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QMutex>
#include <QMutexLocker>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(lcDpDatabase, "net.drawpile.database", QtWarningMsg)

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

QSqlDatabase getConnection(
	const QString &connectionName, const QString &fileName,
	const QString &sourceFileName)
{
	if(QSqlDatabase::contains(connectionName)) {
		return QSqlDatabase::database(connectionName);
	} else {
		QString databasePath = utils::paths::writablePath(fileName);
		QFileInfo fileInfo(databasePath);
		if(!sourceFileName.isEmpty()) {
			copyFrom(databasePath, sourceFileName, fileInfo);
		}
		fixFilePermissions(databasePath, fileInfo);
		QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
		db.setDatabaseName(databasePath);
		if(!db.open()) {
			qCWarning(
				lcDpDatabase, "Can't open database at '%s'",
				qUtf8Printable(db.databaseName()));
		}
		return db;
	}
}

}

QSqlDatabase sqlite(
	const QString &connectionName, const QString &humaneName,
	const QString &fileName, const QString &sourceFileName)
{
	static QMutex mutex;
	QMutexLocker lock{&mutex};
	QSqlDatabase db = getConnection(connectionName, fileName, sourceFileName);
	if(db.isValid() && db.isOpen()) {
		return db;
	} else {
		qCWarning(
			lcDpDatabase, "Database '%s' not available",
			qUtf8Printable(humaneName));
		return QSqlDatabase{};
	}
}

bool prepare(QSqlQuery &query, const QString &sql)
{
	if(query.prepare(sql)) {
		return true;
	} else {
		qCWarning(
			lcDpDatabase, "Error preparing statement '%s': %s",
			qUtf8Printable(sql), qUtf8Printable(query.lastError().text()));
		return false;
	}
}

bool bindValue(QSqlQuery &query, int i, const QVariant &value)
{
	query.bindValue(i, value);
	return true;
}

bool execPrepared(QSqlQuery &query, const QString &sql)
{
	if(query.exec()) {
		return true;
	} else {
		qWarning(
			lcDpDatabase, "Error executing statement '%s': %s",
			qUtf8Printable(sql), qUtf8Printable(query.lastError().text()));
		return false;
	}
}


bool execBatch(QSqlQuery &query, const QString &sql)
{
	if(query.execBatch()) {
		return true;
	} else {
		qWarning(
			lcDpDatabase, "Error executing batch '%s': %s", qUtf8Printable(sql),
			qUtf8Printable(query.lastError().text()));
		return false;
	}
}

bool exec(QSqlQuery &query, const QString &sql, const QVariantList &params)
{
	if(prepare(query, sql)) {
		int paramCount = params.size();
		for(int i = 0; i < paramCount; ++i) {
			query.bindValue(i, params[i]);
		}
		return execPrepared(query, sql);
	} else {
		return false;
	}
}

bool tx(QSqlDatabase &db, std::function<bool()> fn)
{
	if(!db.transaction()) {
		qWarning(
			"Error opening transaction on %s",
			qUtf8Printable(db.databaseName()));
		return false;
	}

	bool ok = fn();

	if(ok) {
		if(db.commit()) {
			return true;
		} else {
			qWarning(
				"Error committing transaction on %s",
				qUtf8Printable(db.databaseName()));
			return false;
		}
	} else {
		if(!db.rollback()) {
			qWarning(
				"Error rolling back transaction on %s",
				qUtf8Printable(db.databaseName()));
		}
		return false;
	}
}

}
}
