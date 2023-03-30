/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016 Calle Laakkonen

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

#include "thinsrv/database.h"
#include "thinsrv/dblog.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/validators.h"
#include "libserver/serverlog.h"
#include "libshared/util/qtcompat.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QVariant>
#include <QUrl>
#include <QRegularExpression>
#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

namespace server {

struct Database::Private {
	QSqlDatabase db;
	ServerLog *logger;
};

static bool initDatabase(QSqlDatabase db)
{
	QSqlQuery q(db);

	// Settings key/value table
	if(!q.exec(
		"CREATE TABLE IF NOT EXISTS settings (key PRIMARY KEY, value);"
		))
		return false;

	// Listing server URL whitelist (regular expressions)
	if(!q.exec(
		"CREATE TABLE IF NOT EXISTS listingservers (url);"
		))
		return false;

	// List of serverwide IP address bans
	if(!q.exec(
		"CREATE TABLE IF NOT EXISTS ipbans (ip, subnet, expires, comment, added);"
		))
		return false;

	// Registered user accounts
	if(!q.exec(
		"CREATE TABLE IF NOT EXISTS users ("
			"username UNIQUE," // the username
			"password,"        // hashed password
			"locked,"          // is this username locked/banned
			"flags"            // comma separated list of extra features (e.g. "mod")
			");"
		))
		return false;

	return true;
}

Database::Database(QObject *parent)
	: ServerConfig(parent), d(new Private)
{
	// Temporary logger until DB log is ready
	d->logger = new InMemoryLog;

	// Periodic task scheduler
	QTimer *dailyTimer = new QTimer(this);
	dailyTimer->setTimerType(Qt::VeryCoarseTimer);
	dailyTimer->setSingleShot(false);
	dailyTimer->setInterval(24 * 60 * 60 * 1000);
	connect(dailyTimer, &QTimer::timeout, this, &Database::dailyTasks);
	dailyTimer->start();
}

Database::~Database()
{
	delete d->logger;
	delete d;
}

bool Database::openFile(const QString &path)
{
	d->db = QSqlDatabase::addDatabase("QSQLITE");
	d->db.setDatabaseName(path);
	if(!d->db.open()) {
		qCritical("Unable to open database: %s", qPrintable(path));
		return false;
	}

	if(!initDatabase(d->db)) {
		qCritical("Database initialization failed: %s", qPrintable(path));
		return false;
	}

	DbLog *dblog = new DbLog(d->db);
	if(!dblog->initDb()) {
		qWarning("Couldn't initialize database log!");
		delete dblog;
	} else {
		delete d->logger;
		d->logger = dblog;
	}

	qDebug("Opened configuration database: %s", qPrintable(path));

	// Purge old log entries on startup
	dailyTasks();

	return true;
}

void Database::setConfigValue(ConfigKey key, const QString &value)
{
	QSqlQuery q(d->db);
	q.prepare("INSERT OR REPLACE INTO settings VALUES (?, ?)");
	q.bindValue(0, key.name);
	q.bindValue(1, value);
	q.exec();
}

QString Database::getConfigValue(const ConfigKey key, bool &found) const
{
	QSqlQuery q(d->db);
	q.prepare("SELECT value FROM settings WHERE key=?");
	q.bindValue(0, key.name);
	q.exec();
	if(q.next()) {
		found = true;
		return q.value(0).toString();
	} else {
		found = false;
		return QString();
	}
}

bool Database::isAllowedAnnouncementUrl(const QUrl &url) const
{
	if(!url.isValid())
		return false;

	// If whitelisting is not enabled, allow all URLs
	if(!getConfigBool(config::AnnounceWhiteList))
		return true;

	const QString urlStr = url.toString();

	QSqlQuery q(d->db);
	q.exec("SELECT url FROM listingservers");
	while(q.next()) {
		const QString serverUrl = q.value(0).toString();
		const QRegularExpression re(serverUrl);
		if(!re.isValid()) {
			qWarning("Invalid listingserver whitelist regexp: %s", qPrintable(serverUrl));
		} else {
			if(re.match(urlStr).hasMatch())
				return true;
		}
	}

	return false;
}

QStringList Database::listServerWhitelist() const
{
	QStringList list;
	QSqlQuery q(d->db);
	q.exec("SELECT url FROM listingservers");
	while(q.next()) {
		list << q.value(0).toString();
	}
	return list;
}

void Database::updateListServerWhitelist(const QStringList &whitelist)
{
	QSqlQuery q(d->db);
	q.exec("BEGIN TRANSACTION");
	q.exec("DELETE FROM listingservers");
	if(!whitelist.isEmpty()) {
		q.prepare("INSERT INTO listingservers VALUES (?)");
		q.addBindValue(whitelist);
		if(!q.execBatch()) {
			qWarning("Error inserting whitelist");
			q.exec("ROLLBACK");
			return;
		}
	}
	q.exec("COMMIT");
}

bool Database::isAddressBanned(const QHostAddress &addr) const
{
	QSqlQuery q(d->db);
	q.exec("SELECT ip, subnet FROM ipbans WHERE expires > datetime('now')");

	while(q.next()) {
		const QHostAddress a(q.value(0).toString());
		int subnet = q.value(1).toInt();
		if(subnet==0) {
			switch(a.protocol()) {
			case QAbstractSocket::IPv4Protocol: subnet=32; break;
			case QAbstractSocket::IPv6Protocol: subnet=128; break;
			default: break;
			}
		}

		if(addr.isInSubnet(a, subnet))
			return true;
	}

	return false;
}

static QJsonObject banResultToJson(const QSqlQuery &q)
{
	QJsonObject b;
	b["id"] = q.value(0).toInt();
	b["ip"] = q.value(1).toString();
	b["subnet"] = q.value(2).toInt();
	b["expires"] = q.value(3).toString();
	b["comment"] = q.value(4).toString();
	b["added"] = q.value(5).toString();
	return b;
}

QJsonArray Database::getBanlist() const
{
	QJsonArray result;
	QSqlQuery q(d->db);
	q.exec("SELECT rowid, ip, subnet, expires, comment, added FROM ipbans");

	while(q.next()) {
		result.append(banResultToJson(q));
	}
	return result;
}

QJsonObject Database::addBan(const QHostAddress &ip, int subnet, const QDateTime &expiration, const QString &comment)
{
	QSqlQuery q(d->db);
	q.prepare("SELECT rowid, ip, subnet, expires, comment, added FROM ipbans WHERE ip=? AND subnet=?");
	q.bindValue(0, ip.toString());
	q.bindValue(1, subnet);
	q.exec();
	if(q.next()) {
		// Matching entry already in database
		return banResultToJson(q);
	} else {
		const QString datefmt = "yyyy-MM-dd HH:mm:ss";
		QString datestr = expiration.toString(datefmt);
		QString now = QDateTime::currentDateTime().toString(datefmt);

		q.prepare("INSERT INTO ipbans (ip, subnet, expires, comment, added) VALUES (?, ?, ?, ?, ?)");
		q.bindValue(0, ip.toString());
		q.bindValue(1, subnet);
		q.bindValue(2, datestr);
		q.bindValue(3, comment);
		q.bindValue(4, now);
		q.exec();

		QJsonObject b;
		b["id"] = q.lastInsertId().toInt();
		b["ip"] = ip.toString();
		b["subnet"] = subnet;
		b["expires"] = datestr;
		b["comment"] = comment;
		b["added"] = now;
		return b;
	}
}

bool Database::deleteBan(int entryId)
{
	QSqlQuery q(d->db);
	q.prepare("DELETE FROM ipbans WHERE rowid=?");
	q.bindValue(0, entryId);
	q.exec();
	return q.numRowsAffected()>0;
}

ServerLog *Database::logger() const
{
	return d->logger;
}

RegisteredUser Database::getUserAccount(const QString &username, const QString &password) const
{
	QSqlQuery q(d->db);
	q.prepare("SELECT rowid, password, locked, flags FROM users WHERE username=?");
	q.bindValue(0, username);
	q.exec();
	if(q.next()) {
		const int rowid = q.value(0).toInt();
		const QByteArray passwordHash = q.value(1).toByteArray();
		const int locked = q.value(2).toInt();
		const QStringList flags = q.value(3).toString().split(',', compat::SkipEmptyParts);

		if(locked) {
			return RegisteredUser {
				RegisteredUser::Banned,
				username,
				QStringList(),
				QString()
			};
		}

		if(!passwordhash::check(password, passwordHash)) {
			return RegisteredUser {
				RegisteredUser::BadPass,
				username,
				QStringList(),
				QString()
			};
		}

		return RegisteredUser {
			RegisteredUser::Ok,
			username,
			flags,
			QString::number(rowid)
		};
	} else {
		return RegisteredUser {
			RegisteredUser::NotFound,
			username,
			QStringList(),
			QString()
		};
	}
}

static QJsonObject userQueryToJson(const QSqlQuery &q)
{
	QJsonObject o;
	o["id"] = q.value(0).toInt();
	o["username"] = q.value(1).toString();
	o["locked"] = q.value(2).toBool();
	o["flags"] = q.value(3).toString();
	return o;
}

QJsonArray Database::getAccountList() const
{
	QJsonArray list;
	QSqlQuery q(d->db);
	q.exec("SELECT rowid, username, locked, flags FROM users");
	while(q.next()) {
		list << userQueryToJson(q);
	}
	return list;
}

QJsonObject Database::addAccount(const QString &username, const QString &password, bool locked, const QStringList &flags)
{
	if(!validateUsername(username))
		return QJsonObject();

	QSqlQuery q(d->db);
	q.prepare("INSERT INTO users (username, password, locked, flags) VALUES (?, ?, ?, ?)");
	q.bindValue(0, username);
	q.bindValue(1, passwordhash::hash(password));
	q.bindValue(2, locked);
	q.bindValue(3, flags.join(','));
	if(q.exec()) {
		q.exec("SELECT rowid, username, locked, flags FROM users WHERE rowid IN (SELECT last_insert_rowid())");
		if(q.next())
			return userQueryToJson(q);
	}
	qWarning("Error adding user account: %s", qPrintable(q.lastError().text()));
	return QJsonObject();
}

QJsonObject Database::updateAccount(int id, const QJsonObject &update)
{
	QStringList updates;
	QVariantList params;

	if(validateUsername(update["username"].toString())) {
		updates << "username=?";
		params << update["username"].toString();
	}

	if(!update["password"].toString().isEmpty()) {
		updates << "password=?";
		params << passwordhash::hash(update["password"].toString());
	}

	if(update.contains("locked")) {
		updates << "locked=?";
		params << update["locked"].toBool();
	}

	if(update.contains("flags")) {
		updates << "flags=?";
		params << update["flags"].toString();
	}

	QSqlQuery q(d->db);

	if(!updates.isEmpty()) {
		QString sql = QString("UPDATE users SET %1 WHERE rowid=?").arg(updates.join(','));
		q.prepare(sql);
		for(int i=0;i<params.size();++i)
			q.bindValue(i, params[i]);
		q.bindValue(params.size(), id);
		if(!q.exec())
			return QJsonObject();
	}

	q.prepare("SELECT rowid, username, locked, flags FROM users WHERE rowid=?");
	q.bindValue(0, id);
	q.exec();
	if(q.next())
		return userQueryToJson(q);
	qWarning("Error updating user account %d: %s", id, qPrintable(q.lastError().text()));
	return QJsonObject();
}

bool Database::deleteAccount(int userId)
{
	QSqlQuery q(d->db);
	q.prepare("DELETE FROM users WHERE rowid=?");
	q.bindValue(0, userId);
	q.exec();
	return q.numRowsAffected()>0;
}

void Database::dailyTasks()
{
	// Purge old Database log entries
	DbLog *dblog = dynamic_cast<DbLog*>(d->logger);
	if(dblog) {
		const int purged = dblog->purgeLogs(getConfigInt(config::LogPurgeDays));
		if(purged > 0)
			qInfo("Purged %d old log entries", purged);
	}
}

}
