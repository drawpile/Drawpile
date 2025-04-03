// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/accountlistmodel.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/statedatabase.h"
#include "libclient/utils/wasmpersistence.h"
#include "libshared/util/qtcompat.h"
#include <QIcon>
#include <algorithm>
#ifdef HAVE_QTKEYCHAIN
#	if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#		include <qt6keychain/keychain.h>
#	else
#		include <qt5keychain/keychain.h>
#	endif
#endif

AccountListModel::AccountListModel(
	utils::StateDatabase &state, AvatarListModel *avatars, QObject *parent)
	: QAbstractListModel(parent)
	, m_state(state)
	, m_avatars(avatars)
{
	createTables();
}

int AccountListModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_accounts.size();
}

QVariant AccountListModel::data(const QModelIndex &idx, int role) const
{
	compat::sizetype i;
	if(idx.isValid() && (i = idx.row()) >= 0 && i < m_accounts.size()) {
		switch(role) {
		case Qt::DisplayRole:
			return QStringLiteral(" %1").arg(m_accounts[i].displayUsername);
		case Qt::DecorationRole: {
			const Account &account = m_accounts[i];
			QVariant avatar = m_avatars
								  ? m_avatars->getAvatar(account.avatarFilename)
										.data(Qt::DecorationRole)
								  : QVariant();
			if(avatar.isNull() || avatar.value<QPixmap>().isNull()) {
				QIcon icon;
				if(account.type == Type::ExtAuth) {
					if(account.host == QStringLiteral("drawpile.net")) {
						icon = QIcon(QStringLiteral(":/icons/drawpile.png"));
					} else {
						icon = QIcon::fromTheme(QStringLiteral("im-user"));
					}
				} else {
					icon = QIcon::fromTheme(QStringLiteral("network-server"));
				}
				return icon.pixmap(32, 32);
			} else {
				return avatar;
			}
		}
		case TypeRole:
			return QVariant::fromValue(m_accounts[i].type);
		case DisplayUsernameRole:
			return m_accounts[i].displayUsername;
		case AvatarFilenameRole:
			return m_accounts[i].avatarFilename;
		default:
			break;
		}
	}
	return QVariant();
}

bool AccountListModel::load(const QUrl &url, const QUrl &extAuthUrl)
{
	QVector<Account> accounts;
	QSet<QString> usernames;

	QString authHost = url.host(QUrl::FullyEncoded).toCaseFolded();
	if(!authHost.isEmpty()) {
		loadAccounts(accounts, usernames, Type::Auth, authHost);
	}

	QString extAuthHost;
	if(extAuthUrl.isValid()) {
		extAuthHost = extAuthUrl.host(QUrl::FullyEncoded).toCaseFolded();
		loadAccounts(accounts, usernames, Type::ExtAuth, extAuthHost);
	}

	std::sort(accounts.begin(), accounts.end(), isAccountLessThan);

	bool hasChanged = m_authHost != authHost || m_extAuthHost != extAuthHost ||
					  m_accounts != accounts;
	if(hasChanged) {
		beginResetModel();
		m_authHost = authHost;
		m_extAuthHost = extAuthHost;
		m_accounts = accounts;
		endResetModel();
		return true;
	} else {
		return false;
	}
}

int AccountListModel::getMostRecentIndex() const
{
	compat::sizetype count = m_accounts.size();
	if(count == 0) {
		return -1;
	} else {
		int bestIndex = 0;
		QDateTime bestLastUsed = m_accounts[0].lastUsed;
		for(compat::sizetype i = 1; i < count; ++i) {
			QDateTime lastUsed = m_accounts[i].lastUsed;
			if(lastUsed > bestLastUsed) {
				bestIndex = i;
				bestLastUsed = lastUsed;
			}
		}
		return bestIndex;
	}
}

bool AccountListModel::saveAccount(
	Type type, const QString &displayUsername, const QString &avatarFilename)
{
	if(displayUsername.isEmpty()) {
		qWarning("Attempt to save account with empty username");
		return false;
	}

	QString host;
	switch(type) {
	case Type::Auth:
		host = m_authHost;
		break;
	case Type::ExtAuth:
		host = m_extAuthHost;
		break;
	}
	if(host.isEmpty()) {
		qWarning(
			"Attempt to save account for '%s' on uninitialized type %d",
			qUtf8Printable(displayUsername), int(type));
		return false;
	}

	QString username = normalizeUsername(displayUsername);
	QDateTime lastUsed = QDateTime::currentDateTimeUtc();
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);

	QString sql = QStringLiteral(
		"insert into accounts (\n"
		"	type, host, username, display_username, avatar_filename,\n"
		"	last_used)\n"
		"values (?, ?, ?, ?, ?, ?)\n"
		"on conflict do update set\n"
		"	display_username = excluded.display_username,\n"
		"	avatar_filename = excluded.avatar_filename,\n"
		"	last_used = excluded.last_used");
	if(!m_state.query().exec(
		   sql,
		   {int(type), host, displayUsername.toCaseFolded(), displayUsername,
			avatarFilename.isNull() ? QStringLiteral("") : avatarFilename,
			lastUsed.toString(Qt::ISODate)})) {
		return false;
	}

	return true;
}

void AccountListModel::savePassword(
	const QString &password, const QString &keychainSecretName,
	bool insecureFallback)
{
#ifdef HAVE_QTKEYCHAIN
	QKeychain::WritePasswordJob *writeJob =
		new QKeychain::WritePasswordJob(KEYCHAIN_NAME);
	writeJob->setKey(keychainSecretName);
	writeJob->setTextData(password);
	// The write job might finish with an error after we've been destroyed.
	utils::StateDatabase *state = &m_state;
	QString fallbackPassword = insecureFallback ? password : QString();
	connect(
		writeJob, &QKeychain::Job::finished, state,
		[state, fallbackPassword](QKeychain::Job *job) {
			QKeychain::Error error = job->error();
			// QtKeychain isn't very good about detecting if it can actually
			// save passwords. For example, if there's no secrets service in
			// DBus, it will just spew an OtherError instead of saying that
			// a keychain is unavailable. So we have to manually fall back.
			bool fallback = !fallbackPassword.isEmpty() &&
							(error == QKeychain::NoBackendAvailable ||
							 error == QKeychain::OtherError);
			qWarning(
				"Error saving password for '%s': %s (%d)%s",
				qUtf8Printable(job->key()), qUtf8Printable(job->errorString()),
				int(error),
				fallback ? " - falling back to insecure storage"
						 : " - not saving it");
			if(fallback) {
				savePasswordFallback(state, job->key(), fallbackPassword);
			} else {
				DRAWPILE_FS_PERSIST();
			}
		});
	writeJob->start();
#else
	if(insecureFallback) {
		savePasswordFallback(&m_state, keychainSecretName, password);
	} else {
		qWarning(
			"Can't save password under '%s': keychain not available and "
			"insecure fallback disabled",
			qUtf8Printable(keychainSecretName));
	}
#endif
}

void AccountListModel::readAccountPassword(
	int jobId, Type type, const QString &displayUsername)
{
	QString keychainSecretName = buildKeychainSecretName(
		type, hostForType(type), normalizeUsername(displayUsername));
	readPassword(jobId, keychainSecretName);
}

void AccountListModel::readPassword(
	int jobId, const QString &keychainSecretName)
{
#ifdef HAVE_QTKEYCHAIN
	QKeychain::ReadPasswordJob *readJob =
		new QKeychain::ReadPasswordJob(KEYCHAIN_NAME);
	readJob->setKey(keychainSecretName);
	connect(
		readJob, &QKeychain::ReadPasswordJob::finished,
		[keychainSecretName](QKeychain::Job *job) {
			QKeychain::Error error = job->error();
			if(error != QKeychain::NoError &&
			   error != QKeychain::EntryNotFound) {
				qWarning(
					"Error reading password for '%s': %s (%d)",
					qUtf8Printable(keychainSecretName),
					qUtf8Printable(job->errorString()), int(error));
			}
		});
	connect(
		readJob, &QKeychain::ReadPasswordJob::finished, this,
		[this, jobId, keychainSecretName](QKeychain::Job *job) {
			QString password =
				job->error() == QKeychain::NoError
					? static_cast<QKeychain::ReadPasswordJob *>(job)->textData()
					: readPasswordFallback(keychainSecretName);
			emit passwordReadFinished(jobId, password);
		});
	readJob->start();
#else
	emit passwordReadFinished(jobId, readPasswordFallback(keychainSecretName));
#endif
}

void AccountListModel::deleteAccountAt(int i)
{
	if(i >= 0 && i < compat::cast_6<int>(m_accounts.size())) {
		DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
		const Account &account = m_accounts[i];
		QString sql =
			QStringLiteral("delete from accounts\n"
						   "where type = ? and host = ? and username = ?");
		m_state.query().exec(
			sql, {int(account.type), account.host, account.username});

		deletePassword(buildKeychainSecretName(
			account.type, account.host, account.username));

		beginRemoveRows(QModelIndex(), i, i);
		m_accounts.removeAt(i);
		endRemoveRows();
	}
}

void AccountListModel::deleteAccountPassword(
	Type type, const QString &displayUsername)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	QString keychainSecretName = buildKeychainSecretName(
		type, hostForType(type), normalizeUsername(displayUsername));
	deletePassword(keychainSecretName);
}

void AccountListModel::deletePassword(const QString &keychainSecretName)
{
#ifdef HAVE_QTKEYCHAIN
	QKeychain::DeletePasswordJob *deleteJob =
		new QKeychain::DeletePasswordJob(KEYCHAIN_NAME);
	// We don't use the insecure fallback anymore, but try deleting anyway.
	deleteJob->setInsecureFallback(true);
	deleteJob->setKey(keychainSecretName);
	connect(
		deleteJob, &QKeychain::Job::finished,
		[keychainSecretName](QKeychain::Job *job) {
			QKeychain::Error error = job->error();
			if(error != QKeychain::NoError &&
			   error != QKeychain::EntryNotFound) {
				qWarning(
					"Error deleting password for '%s': %s (%d)",
					qUtf8Printable(keychainSecretName),
					qUtf8Printable(job->errorString()), int(error));
			}
			DRAWPILE_FS_PERSIST();
		});
	deleteJob->start();
#endif
	deletePasswordFallback(keychainSecretName);
}

QString AccountListModel::buildKeychainSecretNameFor(
	Type type, const QString &displayUsername)
{
	QString host;
	switch(type) {
	case Type::Auth:
		host = m_authHost;
		break;
	case Type::ExtAuth:
		host = m_extAuthHost;
		break;
	}
	return buildKeychainSecretName(
		type, host, normalizeUsername(displayUsername));
}

QString AccountListModel::buildKeychainSecretName(
	Type type, const QString &host, const QString &username)
{
	QString prefix;
	switch(type) {
	case Type::Auth:
		prefix = QStringLiteral("srv");
		break;
	case Type::ExtAuth:
		prefix = QStringLiteral("ext");
		break;
	}
	return QStringLiteral("%1:%2@%3").arg(prefix, username, host);
}

bool AccountListModel::canSavePasswords(bool insecureFallback)
{
#ifdef HAVE_QTKEYCHAIN
	Q_UNUSED(insecureFallback);
	return true;
#else
	return insecureFallback;
#endif
}

void AccountListModel::clearFallbackPasswords()
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	drawdance::Query qry = m_state.query();
	qry.exec("delete from insecure_storage");
}

void AccountListModel::createTables()
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	drawdance::Query qry = m_state.query();
	qry.exec("create table if not exists accounts ("
			 "type integer not null,"
			 "host text not null,"
			 "username text not null,"
			 "display_username text not null,"
			 "avatar_filename text not null,"
			 "last_used text not null,"
			 "primary key (type, host, username))");
	qry.exec("create table if not exists insecure_storage ("
			 "keychain_secret_name text primary key not null,"
			 "obfuscated_value text not null)");
}

void AccountListModel::loadAccounts(
	QVector<Account> &accounts, QSet<QString> &usernames, Type type,
	const QString &host)
{
	drawdance::Query qry = m_state.query();
	if(qry.exec(
		   "select username, display_username, avatar_filename, last_used "
		   "from accounts where type = ? and host = ?",
		   {int(type), host})) {
		while(qry.next()) {
			QString username = qry.columnText16(0);
			if(!usernames.contains(username)) {
				usernames.insert(username);
				accounts.append({
					type,
					host,
					username,
					qry.columnText16(1),
					qry.columnText16(2),
					QDateTime::fromString(qry.columnText16(3), Qt::ISODate),
				});
			}
		}
	}
}

QString AccountListModel::hostForType(Type type)
{
	switch(type) {
	case Type::Auth:
		return m_authHost;
	case Type::ExtAuth:
		return m_extAuthHost;
	}
	qWarning("Unhandled host for type %d", int(type));
	return QString();
}

QString AccountListModel::normalizeUsername(const QString &displayUsername)
{
	return displayUsername.toCaseFolded();
}

bool AccountListModel::isAccountLessThan(const Account &a, const Account &b)
{
	return a.username.compare(b.username, Qt::CaseInsensitive) < 0;
}

QString
AccountListModel::readPasswordFallback(const QString &keychainSecretName)
{
	drawdance::Query qry = m_state.query();
	bool ok = qry.exec(
				  "select obfuscated_value from insecure_storage "
				  "where keychain_secret_name = ?",
				  {keychainSecretName}) &&
			  qry.next();
	return ok ? deobfuscate(qry.columnText16(0)) : QString();
}

void AccountListModel::savePasswordFallback(
	utils::StateDatabase *state, const QString &keychainSecretName,
	const QString &password)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	drawdance::Query qry = state->query();
	qry.exec(
		"insert into insecure_storage (keychain_secret_name, obfuscated_value) "
		"values (?, ?) on conflict do update set "
		"obfuscated_value = excluded.obfuscated_value",
		{keychainSecretName, obfuscate(password)});
}

void AccountListModel::deletePasswordFallback(const QString &keychainSecretName)
{
	DRAWPILE_FS_PERSIST_SCOPE(scopedFsSync);
	drawdance::Query qry = m_state.query();
	qry.exec(
		"delete from insecure_storage where keychain_secret_name = ?",
		{keychainSecretName});
}

QString AccountListModel::obfuscate(const QString &value)
{
	return QStringLiteral("O1%1").arg(
		QString::fromUtf8(value.toUtf8().toHex()));
}

QString AccountListModel::deobfuscate(const QString &obfuscatedValue)
{
	return obfuscatedValue.startsWith(QStringLiteral("O1"))
			   ? QString::fromUtf8(
					 QByteArray::fromHex(obfuscatedValue.mid(2).toUtf8()))
			   : QString();
}

bool operator==(
	const AccountListModel::Account &a, const AccountListModel::Account &b)
{
	return a.type == b.type && a.host == b.host && a.username == b.username &&
		   a.displayUsername == b.displayUsername &&
		   a.avatarFilename == b.avatarFilename && a.lastUsed == b.lastUsed;
}
