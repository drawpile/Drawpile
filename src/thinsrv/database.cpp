// SPDX-License-Identifier: GPL-3.0-or-later

#include "thinsrv/database.h"
#include "libserver/serverlog.h"
#include "libshared/util/database.h"
#include "libshared/util/passwordhash.h"
#include "libshared/util/qtcompat.h"
#include "libshared/util/validators.h"
#include "thinsrv/dblog.h"
#include "thinsrv/extbans.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTimer>
#include <QUrl>
#include <QVariant>

namespace server {

struct Database::Private {
	QSqlDatabase db;
	InMemoryLog *memlog;
	DbLog *dblog;
};

static bool initDatabase(QSqlDatabase db)
{
	QSqlQuery q(db);

	// Settings key/value table
	if(!utils::db::exec(
		   q, QStringLiteral("CREATE TABLE IF NOT EXISTS settings (key PRIMARY "
							 "KEY, value)"))) {
		return false;
	}

	// Listing server URL whitelist (regular expressions)
	if(!utils::db::exec(
		   q,
		   QStringLiteral("CREATE TABLE IF NOT EXISTS listingservers (url)"))) {
		return false;
	}

	// List of serverwide IP address bans
	if(!utils::db::exec(
		   q, QStringLiteral("CREATE TABLE IF NOT EXISTS ipbans ("
							 "ip, subnet, expires, comment, added)"))) {
		return false;
	}

	// List of serverwide system bans
	if(!utils::db::exec(
		   q, QStringLiteral("CREATE TABLE IF NOT EXISTS systembans ("
							 "id INTEGER PRIMARY KEY NOT NULL,"
							 "sid TEXT NOT NULL,"
							 "reaction TEXT NOT NULL DEFAULT 'normal',"
							 "expires TEXT NOT NULL,"
							 "comment TEXT,"
							 "reason TEXT,"
							 "added TEXT NOT NULL)"))) {
		return false;
	}

	// List of serverwide external user bans
	if(!utils::db::exec(
		   q, QStringLiteral("CREATE TABLE IF NOT EXISTS userbans ("
							 "id INTEGER PRIMARY KEY NOT NULL,"
							 "userid INTEGER NOT NULL,"
							 "reaction TEXT NOT NULL DEFAULT 'normal',"
							 "expires TEXT NOT NULL,"
							 "comment TEXT,"
							 "reason TEXT,"
							 "added TEXT NOT NULL)"))) {
		return false;
	}

	// Registered user accounts
	if(!utils::db::exec(
		   q,
		   QStringLiteral(
			   "CREATE TABLE IF NOT EXISTS users ("
			   "username UNIQUE," // the username
			   "password,"		  // hashed password
			   "locked,"		  // is this username locked/banned
			   "flags)" // comma separated list of extra features (e.g. "mod")
			   ))) {
		return false;
	}

	// Disabling of external, imported bans
	if(!utils::db::exec(
		   q, QStringLiteral("CREATE TABLE IF NOT EXISTS disabledextbans ("
							 "id INTEGER PRIMARY KEY NOT NULL)"))) {
		return false;
	}

	return true;
}

Database::Database(QObject *parent)
	: ServerConfig(parent)
	, d(new Private)
{
	// Temporary logger until DB log is ready
	d->memlog = new InMemoryLog;
	d->dblog = nullptr;

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
	delete d->dblog;
	delete d->memlog;
	delete d;
}

bool Database::openFile(const QString &path)
{
	d->db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"));
	d->db.setDatabaseName(path);
	if(!d->db.open()) {
		qCritical("Unable to open database: %s", qUtf8Printable(path));
		return false;
	}

	if(!initDatabase(d->db)) {
		qCritical("Database initialization failed: %s", qUtf8Printable(path));
		return false;
	}

	DbLog *dblog = new DbLog(d->db);
	if(!dblog->initDb()) {
		qWarning("Couldn't initialize database log!");
		delete dblog;
	} else {
		delete d->memlog;
		delete d->dblog;
		d->memlog = nullptr;
		d->dblog = dblog;
	}

	qDebug("Opened configuration database: %s", qUtf8Printable(path));

	// Purge old log entries on startup
	dailyTasks();

	return true;
}

void Database::loadExternalIpBans(ExtBans *extBans)
{
	extBans->loadFromCache();
	QSqlQuery q(d->db);
	if(utils::db::exec(q, QStringLiteral("SELECT id FROM disabledextbans"))) {
		while(q.next()) {
			ServerConfig::setExternalBanEnabled(q.value(0).toInt(), false);
		}
	}
}

bool Database::setExternalBanEnabled(int id, bool enabled)
{
	QSqlQuery q(d->db);
	QString sql =
		enabled ? QStringLiteral("DELETE FROM disabledextbans WHERE id = ?")
				: QStringLiteral(
					  "INSERT OR REPLACE INTO disabledextbans (id) VALUES (?)");
	return utils::db::exec(q, sql, {id}) &&
		   ServerConfig::setExternalBanEnabled(id, enabled);
}

void Database::setConfigValue(ConfigKey key, const QString &value)
{
	setConfigValueByName(key.name, value);
}

void Database::setConfigValueByName(const QString &name, const QString &value)
{
	QSqlQuery q(d->db);
	utils::db::exec(
		q, QStringLiteral("INSERT OR REPLACE INTO settings VALUES (?, ?)"),
		{name, value});
}

QString Database::getConfigValue(const ConfigKey key, bool &found) const
{
	return getConfigValueByName(key.name, found);
}

QString Database::getConfigValueByName(const QString &name, bool &found) const
{
	QSqlQuery q(d->db);
	if(utils::db::exec(
		   q, QStringLiteral("SELECT value FROM settings WHERE key = ?"),
		   {name}) &&
	   q.next()) {
		found = true;
		return q.value(0).toString();
	} else {
		found = false;
		return QString();
	}
}

bool Database::isAllowedAnnouncementUrl(const QUrl &url) const
{
	if(!url.isValid()) {
		return false;
	}

	// If whitelisting is not enabled, allow all URLs
	if(!getConfigBool(config::AnnounceWhiteList)) {
		return true;
	}

	const QString urlStr = url.toString();

	QSqlQuery q(d->db);
	if(utils::db::exec(q, QStringLiteral("SELECT url FROM listingservers"))) {
		while(q.next()) {
			const QString serverUrl = q.value(0).toString();
			const QRegularExpression re(serverUrl);
			if(!re.isValid()) {
				qWarning(
					"Invalid listingserver whitelist regexp: %s",
					qUtf8Printable(serverUrl));
			} else {
				if(re.match(urlStr).hasMatch()) {
					return true;
				}
			}
		}
	}

	return false;
}

QStringList Database::listServerWhitelist() const
{
	QStringList list;
	QSqlQuery q(d->db);
	if(utils::db::exec(q, QStringLiteral("SELECT url FROM listingservers"))) {
		while(q.next()) {
			list.append(q.value(0).toString());
		}
	}
	return list;
}

void Database::updateListServerWhitelist(const QStringList &whitelist)
{
	utils::db::tx(d->db, [this, &whitelist] {
		QSqlQuery q(d->db);
		if(!utils::db::exec(q, QStringLiteral("DELETE FROM listingservers"))) {
			return false;
		}

		if(whitelist.isEmpty()) {
			return true;
		} else {
			QString sql =
				QStringLiteral("INSERT INTO listingservers VALUES (?)");
			if(!utils::db::prepare(q, sql)) {
				return false;
			}
			q.addBindValue(whitelist);
			return utils::db::execBatch(q, sql);
		}
	});
}

BanResult Database::isAddressBanned(const QHostAddress &addr) const
{
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q, QStringLiteral("SELECT rowid, ip, subnet, expires FROM ipbans\n"
						  "WHERE expires > datetime('now')"));
	if(ok) {
		while(q.next()) {
			QHostAddress ip(q.value(1).toString());
			int subnet = q.value(2).toInt();
			if(matchesBannedAddress(addr, ip, subnet)) {
				return {
					BanReaction::NormalBan,
					QString(),
					parseDateTime(q.value(3).toString()),
					addr.toString(),
					QStringLiteral("database"),
					QStringLiteral("IP"),
					q.value(0).toInt(),
					true};
			}
		}
	}
	return ServerConfig::isAddressBanned(addr);
}

BanResult Database::isSystemBanned(const QString &sid) const
{
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q,
		QStringLiteral("SELECT id, reaction, expires, reason FROM systembans\n"
					   "WHERE sid = ? AND expires > datetime('now')\n"
					   "LIMIT 1"),
		{sid});
	if(ok && q.next()) {
		int id = q.value(0).toLongLong();
		BanReaction reaction = parseReaction(q.value(1).toString());
		QDateTime expires = parseDateTime(q.value(2).toString());
		QString reason = q.value(3).toString();
		return {
			reaction,
			reason,
			expires,
			sid,
			QStringLiteral("database"),
			QStringLiteral("SID"),
			id,
			false};
	}
	return ServerConfig::isSystemBanned(sid);
}

BanResult Database::isUserBanned(long long userId) const
{
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q,
		QStringLiteral("SELECT id, reaction, expires, reason FROM userbans\n"
					   "WHERE userid = ? AND expires > datetime('now')\n"
					   "LIMIT 1"),
		{userId});
	if(ok && q.next()) {
		int id = q.value(0).toLongLong();
		BanReaction reaction = parseReaction(q.value(1).toString());
		QDateTime expires = parseDateTime(q.value(2).toString());
		QString reason = q.value(3).toString();
		return {
			reaction,
			reason,
			expires,
			QString::number(userId),
			QStringLiteral("database"),
			QStringLiteral("User"),
			id,
			false};
	}
	return ServerConfig::isUserBanned(userId);
}

QJsonArray Database::getIpBanlist() const
{
	QJsonArray result;
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q,
		QStringLiteral(
			"SELECT rowid, ip, subnet, expires, comment, added FROM ipbans"));
	while(ok && q.next()) {
		result.append(ipBanResultToJson(q));
	}
	return result;
}

QJsonArray Database::getSystemBanlist() const
{
	QJsonArray result;
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q, QStringLiteral("SELECT id, sid, expires, reaction, reason, comment, "
						  "added FROM systembans ORDER BY id ASC"));
	while(ok && q.next()) {
		result.append(systemBanResultToJson(q));
	}
	return result;
}

QJsonArray Database::getUserBanlist() const
{
	QJsonArray result;
	QSqlQuery q(d->db);
	bool ok = utils::db::exec(
		q, QStringLiteral("SELECT id, userid, expires, reaction, reason, "
						  "comment, added FROM userbans ORDER BY id ASC"));
	while(ok && q.next()) {
		result.append(userBanResultToJson(q));
	}
	return result;
}

QJsonObject Database::addIpBan(
	const QHostAddress &ip, int subnet, const QDateTime &expiration,
	const QString &comment)
{
	QSqlQuery q(d->db);
	QString ipstr = ip.toString();
	if(!utils::db::exec(
		   q,
		   QStringLiteral("SELECT rowid, ip, subnet, expires, comment, added "
						  "FROM ipbans WHERE ip = ? AND subnet = ?"),
		   {ipstr, subnet})) {
		return {};
	}

	if(q.next()) {
		// Matching entry already in database
		return ipBanResultToJson(q);
	} else {
		QString datestr = formatDateTime(expiration);
		QString now = formatDateTime(QDateTime::currentDateTime());

		if(!utils::db::exec(
			   q,
			   QStringLiteral(
				   "INSERT INTO ipbans (ip, subnet, expires, comment, added) "
				   "VALUES (?, ?, ?, ?, ?)"),
			   {ipstr, subnet, datestr, comment, now})) {
			return {};
		}

		QJsonObject b;
		b["id"] = q.lastInsertId().toInt();
		b["ip"] = ipstr;
		b["subnet"] = subnet;
		b["expires"] = datestr;
		b["comment"] = comment;
		b["added"] = now;
		return b;
	}
}

QJsonObject Database::addSystemBan(
	const QString &sid, const QDateTime &expires, BanReaction reaction,
	const QString &reason, const QString &comment)
{
	QSqlQuery q(d->db);
	QString expiresString = formatDateTime(expires);
	QString addedString = formatDateTime(QDateTime::currentDateTime());
	QString reactionString = reactionToString(reaction);

	bool ok = utils::db::exec(
		q,
		QStringLiteral("INSERT INTO systembans (sid, expires, reaction, "
					   "reason, comment, added) VALUES (?, ?, ?, ?, ?, ?)"),
		{sid, expiresString, reactionString, reason, comment, addedString});

	if(ok) {
		return {
			{QStringLiteral("id"), q.lastInsertId().toInt()},
			{QStringLiteral("sid"), sid},
			{QStringLiteral("expires"), expiresString},
			{QStringLiteral("reaction"), reactionString},
			{QStringLiteral("reason"), reason},
			{QStringLiteral("comment"), comment},
			{QStringLiteral("added"), addedString},
		};
	} else {
		return {};
	}
}

QJsonObject Database::addUserBan(
	long long userId, const QDateTime &expires, BanReaction reaction,
	const QString &reason, const QString &comment)
{
	QSqlQuery q(d->db);
	QString expiresString = formatDateTime(expires);
	QString addedString = formatDateTime(QDateTime::currentDateTime());
	QString reactionString = reactionToString(reaction);

	bool ok = utils::db::exec(
		q,
		QStringLiteral("INSERT INTO userbans (userid, expires, reaction, "
					   "reason, comment, added) VALUES (?, ?, ?, ?, ?, ?)"),
		{userId, expiresString, reactionString, reason, comment, addedString});

	if(ok) {
		return {
			{QStringLiteral("id"), q.lastInsertId().toInt()},
			{QStringLiteral("userId"), userId},
			{QStringLiteral("expires"), expiresString},
			{QStringLiteral("reaction"), reactionString},
			{QStringLiteral("reason"), reason},
			{QStringLiteral("comment"), comment},
			{QStringLiteral("added"), addedString},
		};
	} else {
		return {};
	}
}

bool Database::deleteIpBan(int entryId)
{
	QSqlQuery q(d->db);
	return utils::db::exec(
			   q, QStringLiteral("DELETE FROM ipbans WHERE rowid = ?"),
			   {entryId}) &&
		   q.numRowsAffected() > 0;
}

bool Database::deleteSystemBan(int entryId)
{
	QSqlQuery q(d->db);
	return utils::db::exec(
			   q, QStringLiteral("DELETE FROM systembans WHERE id = ?"),
			   {entryId}) &&
		   q.numRowsAffected() > 0;
}

bool Database::deleteUserBan(int entryId)
{
	QSqlQuery q(d->db);
	return utils::db::exec(
			   q, QStringLiteral("DELETE FROM userbans WHERE id = ?"),
			   {entryId}) &&
		   q.numRowsAffected() > 0;
}

QJsonObject Database::ipBanResultToJson(const QSqlQuery &q)
{
	QJsonObject b;
	b[QStringLiteral("id")] = q.value(0).toInt();
	b[QStringLiteral("ip")] = q.value(1).toString();
	b[QStringLiteral("subnet")] = q.value(2).toInt();
	b[QStringLiteral("expires")] = q.value(3).toString();
	b[QStringLiteral("comment")] = q.value(4).toString();
	b[QStringLiteral("added")] = q.value(5).toString();
	return b;
}

QJsonObject Database::systemBanResultToJson(const QSqlQuery &q)
{
	QJsonObject b;
	b[QStringLiteral("id")] = q.value(0).toInt();
	b[QStringLiteral("sid")] = q.value(1).toString();
	b[QStringLiteral("expires")] = q.value(2).toString();
	b[QStringLiteral("reaction")] = q.value(3).toString();
	b[QStringLiteral("reason")] = q.value(4).toString();
	b[QStringLiteral("comment")] = q.value(5).toString();
	b[QStringLiteral("added")] = q.value(6).toString();
	return b;
}

QJsonObject Database::userBanResultToJson(const QSqlQuery &q)
{
	QJsonObject b;
	b[QStringLiteral("id")] = q.value(0).toInt();
	b[QStringLiteral("userId")] = double(q.value(1).toLongLong());
	b[QStringLiteral("expires")] = q.value(2).toString();
	b[QStringLiteral("reaction")] = q.value(3).toString();
	b[QStringLiteral("reason")] = q.value(4).toString();
	b[QStringLiteral("comment")] = q.value(5).toString();
	b[QStringLiteral("added")] = q.value(6).toString();
	return b;
}

ServerLog *Database::logger() const
{
	if(d->dblog) {
		return d->dblog;
	} else {
		return d->memlog;
	}
}

RegisteredUser
Database::getUserAccount(const QString &username, const QString &password) const
{
	QSqlQuery q(d->db);
	if(utils::db::exec(
		   q,
		   QStringLiteral("SELECT rowid, password, locked, flags FROM users "
						  "WHERE username = ?"),
		   {username}) &&
	   q.next()) {
		const int rowid = q.value(0).toInt();
		const QByteArray passwordHash = q.value(1).toByteArray();
		const int locked = q.value(2).toInt();
		const QStringList flags =
			q.value(3).toString().split(',', compat::SkipEmptyParts);

		if(locked) {
			return RegisteredUser{
				RegisteredUser::Banned, username, QStringList(), QString()};
		}

		if(!passwordhash::check(password, passwordHash)) {
			return RegisteredUser{
				RegisteredUser::BadPass, username, QStringList(), QString()};
		}

		return RegisteredUser{
			RegisteredUser::Ok, username, flags, QString::number(rowid)};
	} else {
		return RegisteredUser{
			RegisteredUser::NotFound, username, QStringList(), QString()};
	}
}

bool Database::hasAnyUserAccounts() const
{
	QSqlQuery q(d->db);
	return utils::db::exec(q, QStringLiteral("SELECT 1 FROM users LIMIT 1")) &&
		   q.next();
}

bool Database::supportsAdminSectionLocks() const
{
	return true;
}

bool Database::isAdminSectionLocked(const QString &section) const
{
	QSqlQuery q(d->db);
	return utils::db::exec(
			   q, QStringLiteral("SELECT 1 FROM settings WHERE key = ?"),
			   {QStringLiteral("_lock_admin_section_%1").arg(section)}) &&
		   q.next();
}

bool Database::checkAdminSectionLockPassword(const QString &password) const
{
	QSqlQuery q(d->db);
	if(utils::db::exec(
		   q,
		   QStringLiteral(
			   "SELECT value FROM settings WHERE key = '_lock_admin_hash'"))) {
		QByteArray hash = q.next() ? q.value(0).toByteArray() : QByteArray();
		return !passwordhash::isValidHash(hash) ||
			   (!password.isEmpty() && passwordhash::check(password, hash));
	} else {
		return false;
	}
}

bool Database::setAdminSectionsLocked(
	const QSet<QString> &sections, const QString &password)
{
	QVariantList keys;
	for(const QString &section : sections) {
		keys.append(QStringLiteral("_lock_admin_section_%1").arg(section));
	}
	return utils::db::tx(d->db, [&] {
		QSqlQuery q(d->db);
		QString batchSql =
			QStringLiteral("INSERT INTO settings (key, value) VALUES (?, 1)");
		return utils::db::exec(
				   q, QStringLiteral("DELETE FROM settings "
									 "WHERE INSTR(key, '_lock_admin_') = 1")) &&
			   (sections.isEmpty() || (utils::db::prepare(q, batchSql) &&
									   utils::db::bindValue(q, 0, keys) &&
									   utils::db::execBatch(q, batchSql))) &&
			   (password.isEmpty() ||
				utils::db::exec(
					q,
					QStringLiteral("INSERT INTO settings (key, value) "
								   "VALUES ('_lock_admin_hash', ?)"),
					{passwordhash::hash(password)}));
	});
}

static QJsonObject userQueryToJson(const QSqlQuery &q)
{
	QJsonObject o;
	o[QStringLiteral("id")] = q.value(0).toInt();
	o[QStringLiteral("username")] = q.value(1).toString();
	o[QStringLiteral("locked")] = q.value(2).toBool();
	o[QStringLiteral("flags")] = q.value(3).toString();
	return o;
}

QJsonArray Database::getAccountList() const
{
	QJsonArray list;
	QSqlQuery q(d->db);
	if(utils::db::exec(
		   q, QStringLiteral(
				  "SELECT rowid, username, locked, flags FROM users"))) {
		while(q.next()) {
			list.append(userQueryToJson(q));
		}
	}
	return list;
}

QJsonObject Database::addAccount(
	const QString &username, const QString &password, bool locked,
	const QStringList &flags)
{
	if(!validateUsername(username)) {
		return QJsonObject();
	}

	QSqlQuery q(d->db);
	if(utils::db::exec(
		   q,
		   QStringLiteral(
			   "INSERT INTO users (username, password, locked, flags) "
			   "VALUES (?, ?, ?, ?)"),
		   {username, passwordhash::hash(password), locked, flags.join(',')}) &&
	   utils::db::exec(
		   q, QStringLiteral("SELECT rowid, username, locked, flags FROM users "
							 "WHERE rowid IN (SELECT last_insert_rowid())")) &&
	   q.next()) {
		return userQueryToJson(q);
	}
	return QJsonObject();
}

QJsonObject Database::updateAccount(int id, const QJsonObject &update)
{
	QStringList updates;
	QVariantList params;

	QString username = update.value(QStringLiteral("username")).toString();
	if(validateUsername(username)) {
		updates.append(QStringLiteral("username = ?"));
		params.append(username);
	}

	QString password = update.value(QStringLiteral("password")).toString();
	if(!password.isEmpty()) {
		updates.append(QStringLiteral("password = ?"));
		params.append(passwordhash::hash(password));
	}

	if(update.contains(QStringLiteral("locked"))) {
		updates.append(QStringLiteral("locked = ?"));
		params.append(update.value(QStringLiteral("locked")).toBool());
	}

	if(update.contains(QStringLiteral("flags"))) {
		updates.append(QStringLiteral("flags = ?"));
		params.append(update.value(QStringLiteral("flags")).toString());
	}

	QSqlQuery q(d->db);
	if(!updates.isEmpty()) {
		QString sql = QStringLiteral("UPDATE users SET %1 WHERE rowid = ?")
						  .arg(updates.join(','));
		params.append(id);
		if(!utils::db::exec(q, sql, params)) {
			return QJsonObject();
		}
	}

	if(utils::db::exec(
		   q,
		   QStringLiteral("SELECT rowid, username, locked, flags "
						  "FROM users WHERE rowid = ?"),
		   {id}) &&
	   q.next()) {
		return userQueryToJson(q);
	} else {
		return QJsonObject();
	}
}

bool Database::deleteAccount(int userId)
{
	QSqlQuery q(d->db);
	return utils::db::exec(
			   q, QStringLiteral("DELETE FROM users WHERE rowid = ?"),
			   {userId}) &&
		   q.numRowsAffected() > 0;
}

void Database::dailyTasks()
{
	// Purge old Database log entries
	if(d->dblog) {
		int purged = d->dblog->purgeLogs(getConfigInt(config::LogPurgeDays));
		if(purged > 0) {
			qInfo("Purged %d old log entries", purged);
		}
	}
}

}
