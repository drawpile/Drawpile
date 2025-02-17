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
#include <QTimer>
#include <QUrl>
#include <QVariant>
#include <dpdb/sql_qt.h>

namespace server {

struct Database::Private {
	drawdance::Database db;
	InMemoryLog *memlog;
	DbLog *dblog;
};

static bool initDatabase(drawdance::Database &db)
{
	drawdance::Query query = db.queryWithoutLock();
	query.enableWalMode();
	query.setForeignKeysEnabled(false);
	return query.tx([&query] {
		return query.exec("create table if not exists settings ("
						  "key primary key, value)") &&
			   query.exec("create table if not exists listingservers (url)") &&
			   query.exec("create table if not exists ipbans ("
						  "ip, subnet, expires, comment, added)") &&
			   query.exec("create table if not exists systembans ("
						  "id integer primary key not null,"
						  "sid text not null,"
						  "reaction text not null default 'normal',"
						  "expires text not null,"
						  "comment text,"
						  "reason text,"
						  "added text not null)") &&
			   query.exec("create table if not exists userbans ("
						  "id integer primary key not null,"
						  "userid integer not null,"
						  "reaction text not null default 'normal',"
						  "expires text not null,"
						  "comment text,"
						  "reason text,"
						  "added text not null)") &&
			   query.exec("create table if not exists users ("
						  "username unique, password, locked, flags)") &&
			   query.exec("create table if not exists disabledextbans ("
						  "id integer primary key not null)");
	});
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
	d->db.close();
	delete d;
}

bool Database::openFile(const QString &path)
{
	if(!d->db.open(path, QStringLiteral("server database"))) {
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
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select id from disabledextbans")) {
		while(query.next()) {
			ServerConfig::setExternalBanEnabled(query.columnInt(0), false);
		}
	}
}

bool Database::setExternalBanEnabled(int id, bool enabled)
{
	const char *sql =
		enabled ? "delete from disabledextbans where id = ?"
				: "insert or replace into disabledextbans (id) values (?)";
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec(sql, {id}) &&
		   ServerConfig::setExternalBanEnabled(id, enabled);
}

void Database::setConfigValue(ConfigKey key, const QString &value)
{
	setConfigValueByName(key.name, value);
}

void Database::setConfigValueByName(const QString &name, const QString &value)
{
	drawdance::Query query = d->db.queryWithoutLock();
	query.exec("insert or replace into settings values (?, ?)", {name, value});
}

QString Database::getConfigValue(const ConfigKey key, bool &found) const
{
	return getConfigValueByName(key.name, found);
}

QString Database::getConfigValueByName(const QString &name, bool &found) const
{
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select value from settings where key = ?", {name}) &&
	   query.next()) {
		found = true;
		return query.columnText16(0);
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

	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select url from listingservers")) {
		while(query.next()) {
			QString serverUrl = query.columnText16(0);
			QRegularExpression re(serverUrl);
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
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select url from listingservers")) {
		while(query.next()) {
			list.append(query.columnText16(0));
		}
	}
	return list;
}

void Database::updateListServerWhitelist(const QStringList &whitelist)
{
	d->db.txWithoutLock([&whitelist](drawdance::Query &query) {
		if(!query.exec("delete from listingservers")) {
			return false;
		}

		if(!whitelist.isEmpty()) {
			if(!query.prepare("insert into listingservers values (?)")) {
				return false;
			}
			for(const QString &serverUrl : whitelist) {
				if(!query.bind(0, serverUrl) || !query.execPrepared()) {
					return false;
				}
			}
		}
		return true;
	});
}

BanResult Database::isAddressBanned(const QHostAddress &addr) const
{
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select rowid, ip, subnet, expires from ipbans "
				  "where expires > datetime('now')")) {
		while(query.next()) {
			QHostAddress ip(query.columnText16(1));
			int subnet = query.columnInt(2);
			if(matchesBannedAddress(addr, ip, subnet)) {
				return {
					BanReaction::NormalBan,
					QString(),
					parseDateTime(query.columnText16(3)),
					addr.toString(),
					QStringLiteral("database"),
					QStringLiteral("IP"),
					query.columnInt(0),
					true};
			}
		}
	}
	return ServerConfig::isAddressBanned(addr);
}

BanResult Database::isSystemBanned(const QString &sid) const
{
	drawdance::Query query = d->db.queryWithoutLock();
	bool ok = query.exec(
		"select id, reaction, expires, reason from systembans "
		"where sid = ? and expires > datetime('now') limit 1",
		{sid});
	if(ok && query.next()) {
		int id = query.columnInt64(0);
		BanReaction reaction = parseReaction(query.columnText16(1));
		QDateTime expires = parseDateTime(query.columnText16(2));
		QString reason = query.columnText16(3);
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
	drawdance::Query query = d->db.queryWithoutLock();
	bool ok = query.exec(
		"select id, reaction, expires, reason from userbans "
		"where userid = ? and expires > datetime('now') limit 1",
		{userId});
	if(ok && query.next()) {
		int id = query.columnInt64(0);
		BanReaction reaction = parseReaction(query.columnText16(1));
		QDateTime expires = parseDateTime(query.columnText16(2));
		QString reason = query.columnText16(3);
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
	drawdance::Query query = d->db.queryWithoutLock();
	bool ok = query.exec(
		"select rowid, ip, subnet, expires, comment, added from ipbans");
	while(ok && query.next()) {
		result.append(ipBanResultToJson(query));
	}
	return result;
}

QJsonArray Database::getSystemBanlist() const
{
	QJsonArray result;
	drawdance::Query query = d->db.queryWithoutLock();
	bool ok = query.exec("select id, sid, expires, reaction, reason, comment, "
						 "added from systembans order by id asc");
	while(ok && query.next()) {
		result.append(systemBanResultToJson(query));
	}
	return result;
}

QJsonArray Database::getUserBanlist() const
{
	QJsonArray result;
	drawdance::Query query = d->db.queryWithoutLock();
	bool ok = query.exec("select id, userid, expires, reaction, reason, "
						 "comment, added from userbans order by id asc");
	while(ok && query.next()) {
		result.append(userBanResultToJson(query));
	}
	return result;
}

QJsonObject Database::addIpBan(
	const QHostAddress &ip, int subnet, const QDateTime &expiration,
	const QString &comment)
{
	drawdance::Query query = d->db.queryWithoutLock();
	QString ipstr = ip.toString();
	if(!query.exec(
		   "select rowid, ip, subnet, expires, comment, added "
		   "from ipbans where ip = ? and subnet = ?",
		   {ipstr, subnet})) {
		return {};
	}

	if(query.next()) {
		// Matching entry already in database
		return ipBanResultToJson(query);
	} else {
		QString datestr = formatDateTime(expiration);
		QString now = formatDateTime(QDateTime::currentDateTime());

		if(!query.exec(
			   "insert into ipbans (ip, subnet, expires, comment, added) "
			   "values (?, ?, ?, ?, ?)",
			   {ipstr, subnet, datestr, comment, now})) {
			return {};
		}

		QJsonObject b;
		b["id"] = query.lastInsertId();
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
	drawdance::Query query = d->db.queryWithoutLock();
	QString expiresString = formatDateTime(expires);
	QString addedString = formatDateTime(QDateTime::currentDateTime());
	QString reactionString = reactionToString(reaction);

	bool ok = query.exec(
		"insert into systembans (sid, expires, reaction, "
		"reason, comment, added) values (?, ?, ?, ?, ?, ?)",
		{sid, expiresString, reactionString, reason, comment, addedString});

	if(ok) {
		return {
			{QStringLiteral("id"), query.lastInsertId()},
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
	drawdance::Query query = d->db.queryWithoutLock();
	QString expiresString = formatDateTime(expires);
	QString addedString = formatDateTime(QDateTime::currentDateTime());
	QString reactionString = reactionToString(reaction);

	bool ok = query.exec(
		"insert into userbans (userid, expires, reaction, "
		"reason, comment, added) values (?, ?, ?, ?, ?, ?)",
		{userId, expiresString, reactionString, reason, comment, addedString});

	if(ok) {
		return {
			{QStringLiteral("id"), query.lastInsertId()},
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
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec("delete from ipbans where rowid = ?", {entryId}) &&
		   query.numRowsAffected() > 0;
}

bool Database::deleteSystemBan(int entryId)
{
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec("delete from systembans where id = ?", {entryId}) &&
		   query.numRowsAffected() > 0;
}

bool Database::deleteUserBan(int entryId)
{
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec("delete from userbans where id = ?", {entryId}) &&
		   query.numRowsAffected() > 0;
}

QJsonObject Database::ipBanResultToJson(const drawdance::Query &query)
{
	return QJsonObject({
		{QStringLiteral("id"), query.columnInt(0)},
		{QStringLiteral("ip"), query.columnText16(1)},
		{QStringLiteral("subnet"), query.columnInt(2)},
		{QStringLiteral("expires"), query.columnText16(3)},
		{QStringLiteral("comment"), query.columnText16(4)},
		{QStringLiteral("added"), query.columnText16(5)},
	});
}

QJsonObject Database::systemBanResultToJson(const drawdance::Query &query)
{
	return QJsonObject({
		{QStringLiteral("id"), query.columnInt(0)},
		{QStringLiteral("sid"), query.columnText16(1)},
		{QStringLiteral("expires"), query.columnText16(2)},
		{QStringLiteral("reaction"), query.columnText16(3)},
		{QStringLiteral("reason"), query.columnText16(4)},
		{QStringLiteral("comment"), query.columnText16(5)},
		{QStringLiteral("added"), query.columnText16(6)},
	});
}

QJsonObject Database::userBanResultToJson(const drawdance::Query &query)
{
	return QJsonObject({
		{QStringLiteral("id"), query.columnInt(0)},
		{QStringLiteral("userId"), double(query.columnInt64(1))},
		{QStringLiteral("expires"), query.columnText16(2)},
		{QStringLiteral("reaction"), query.columnText16(3)},
		{QStringLiteral("reason"), query.columnText16(4)},
		{QStringLiteral("comment"), query.columnText16(5)},
		{QStringLiteral("added"), query.columnText16(6)},
	});
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
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec(
		   "select rowid, password, locked, flags "
		   "from users where username = ?",
		   {username}) &&
	   query.next()) {
		int rowid = query.columnInt(0);
		QByteArray passwordHash = query.columnBlob(1);
		int locked = query.columnInt(2);
		QStringList flags =
			query.columnText16(3).split(',', compat::SkipEmptyParts);

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
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec("select 1 from users limit 1") && query.next();
}

bool Database::supportsAdminSectionLocks() const
{
	return true;
}

bool Database::isAdminSectionLocked(const QString &section) const
{
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec(
			   "select 1 from settings where key = ?",
			   {QStringLiteral("_lock_admin_section_%1").arg(section)}) &&
		   query.next();
}

bool Database::checkAdminSectionLockPassword(const QString &password) const
{
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec(
		   "select value from settings where key = '_lock_admin_hash'")) {
		QByteArray hash =
			query.next() ? query.columnBlob(0, true) : QByteArray();
		return !passwordhash::isValidHash(hash) ||
			   (!password.isEmpty() && passwordhash::check(password, hash));
	} else {
		return false;
	}
}

bool Database::setAdminSectionsLocked(
	const QSet<QString> &sections, const QString &password)
{
	return d->db.txWithoutLock([&sections, &password](drawdance::Query &query) {
		if(!query.exec(
			   "delete from settings where instr(key, '_lock_admin_') = 1")) {
			return false;
		}

		if(!sections.isEmpty()) {
			if(!query.prepare(
				   "insert into settings (key, value) values (?, 1)")) {
				return false;
			}
			for(const QString &section : sections) {
				QString key =
					QStringLiteral("_lock_admin_section_%1").arg(section);
				if(!query.bind(0, key) || !query.execPrepared()) {
					return false;
				}
			}
		}

		if(!password.isEmpty() && !query.exec(
									  "insert into settings (key, value) "
									  "values ('_lock_admin_hash', ?)",
									  {passwordhash::hash(password)})) {
			return false;
		}

		return true;
	});
}

static QJsonObject userQueryToJson(const drawdance::Query &query)
{
	return QJsonObject({
		{QStringLiteral("id"), query.columnInt(0)},
		{QStringLiteral("username"), query.columnText16(1)},
		{QStringLiteral("locked"), query.columnBool(2)},
		{QStringLiteral("flags"), query.columnText16(3)},
	});
}

QJsonArray Database::getAccountList() const
{
	QJsonArray list;
	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec("select rowid, username, locked, flags from users")) {
		while(query.next()) {
			list.append(userQueryToJson(query));
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

	drawdance::Query query = d->db.queryWithoutLock();
	if(query.exec(
		   "insert into users (username, password, locked, flags) "
		   "values (?, ?, ?, ?)",
		   {username, passwordhash::hash(password), locked, flags.join(',')}) &&
	   query.exec("select rowid, username, locked, flags from users "
				  "where rowid in (select last_insert_rowid())") &&
	   query.next()) {
		return userQueryToJson(query);
	}
	return QJsonObject();
}

QJsonObject Database::updateAccount(int id, const QJsonObject &update)
{
	QStringList updates;
	QVector<drawdance::Query::Param> params;

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

	drawdance::Query query = d->db.queryWithoutLock();
	if(!updates.isEmpty()) {
		QString sql = QStringLiteral("update users set %1 where rowid = ?")
						  .arg(updates.join(','));
		params.append(id);
		if(!query.exec(sql, params)) {
			return QJsonObject();
		}
	}

	if(query.exec(
		   "select rowid, username, locked, flags from users where rowid = ?",
		   {id}) &&
	   query.next()) {
		return userQueryToJson(query);
	} else {
		return QJsonObject();
	}
}

bool Database::deleteAccount(int userId)
{
	drawdance::Query query = d->db.queryWithoutLock();
	return query.exec("delete from users where rowid = ?", {userId}) &&
		   query.numRowsAffected() > 0;
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
