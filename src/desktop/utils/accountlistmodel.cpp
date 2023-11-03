// SPDX-License-Identifier: GPL-3.0-or-later
#include "desktop/utils/accountlistmodel.h"
#include "libclient/utils/avatarlistmodel.h"
#include "libclient/utils/statedatabase.h"
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

void AccountListModel::load(const QUrl &url, const QUrl &extAuthUrl)
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

	std::sort(accounts.begin(), accounts.end(), compareAccounts);

	beginResetModel();
	m_authHost = authHost;
	m_extAuthHost = extAuthHost;
	m_accounts = accounts;
	endResetModel();
}

void AccountListModel::saveAccount(
	Type type, const QString &displayUsername, const QString &password,
	const QString &avatarFilename, bool insecureFallback)
{
	if(displayUsername.isEmpty()) {
		qWarning("Attempt to save account with empty username");
		return;
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
		return;
	}

	QString username = normalizeUsername(displayUsername);
	QDateTime lastUsed = QDateTime::currentDateTimeUtc();

	QSqlQuery qry = m_state.query();
	QString sql = QStringLiteral(
		"insert into accounts (\n"
		"	type, host, username, display_username, avatar_filename,\n"
		"	last_used)\n"
		"values (?, ?, ?, ?, ?, ?)\n"
		"on conflict do update set\n"
		"	display_username = excluded.display_username,\n"
		"	avatar_filename = excluded.avatar_filename,\n"
		"	last_used = excluded.last_used");
	if(!m_state.exec(
		   qry, sql,
		   {int(type), host, displayUsername.toCaseFolded(), displayUsername,
			avatarFilename.isNull() ? QStringLiteral("") : avatarFilename,
			lastUsed.toString(Qt::ISODate)})) {
		return;
	}

	if(!password.isEmpty()) {
		savePassword(
			password, buildKeychainSecretName(type, host, username),
			insecureFallback);
	}
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
		const Account &account = m_accounts[i];
		QSqlQuery qry = m_state.query();
		QString sql =
			QStringLiteral("delete from accounts\n"
						   "where type = ? and host = ? and username = ?");
		m_state.exec(
			qry, sql, {int(account.type), account.host, account.username});

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
		});
	deleteJob->start();
#endif
	deletePasswordFallback(keychainSecretName);
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
	QSqlQuery qry = m_state.query();
	QString sql = QStringLiteral("delete from insecure_storage");
	m_state.exec(qry, sql);
}

void AccountListModel::createTables()
{
	QSqlQuery qry = m_state.query();
	m_state.exec(
		qry, QStringLiteral("create table if not exists accounts (\n"
							"	type integer not null,\n"
							"	host text not null,\n"
							"	username text not null,\n"
							"	display_username text not null,\n"
							"	avatar_filename text not null,\n"
							"	last_used text not null,\n"
							"	primary key (type, host, username))"));
	m_state.exec(
		qry,
		QStringLiteral("create table if not exists insecure_storage (\n"
					   "	keychain_secret_name text primary key not null,\n"
					   "	obfuscated_value text not null)"));
}

void AccountListModel::loadAccounts(
	QVector<Account> &accounts, QSet<QString> &usernames, Type type,
	const QString &host)
{
	QSqlQuery qry = m_state.query();
	QString sql = QStringLiteral(
		"select username, display_username, avatar_filename, last_used\n"
		"from accounts where type = ? and host = ?");
	if(m_state.exec(qry, sql, {int(type), host})) {
		while(qry.next()) {
			QString username = qry.value(0).toString();
			if(!usernames.contains(username)) {
				usernames.insert(username);
				accounts.append({
					type,
					host,
					username,
					qry.value(1).toString(),
					qry.value(2).toString(),
					QDateTime::fromString(qry.value(3).toString(), Qt::ISODate),
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

int AccountListModel::compareAccounts(const Account &a, const Account &b)
{
	if(a.lastUsed < b.lastUsed) {
		return -1;
	} else if(a.lastUsed > b.lastUsed) {
		return 1;
	} else {
		return a.username.compare(b.username, Qt::CaseInsensitive);
	}
}

QString
AccountListModel::readPasswordFallback(const QString &keychainSecretName)
{
	QSqlQuery qry = m_state.query();
	QString sql =
		QStringLiteral("select obfuscated_value from insecure_storage\n"
					   "where keychain_secret_name = ?");
	bool ok = m_state.exec(qry, sql, {keychainSecretName}) && qry.next();
	return ok ? deobfuscate(qry.value(0).toString()) : QString();
}

void AccountListModel::savePasswordFallback(
	utils::StateDatabase *state, const QString &keychainSecretName,
	const QString &password)
{
	QSqlQuery qry = state->query();
	QString sql =
		QStringLiteral("insert into insecure_storage (\n"
					   "	keychain_secret_name, obfuscated_value)\n"
					   "values (?, ?)\n"
					   "on conflict do update set\n"
					   "	obfuscated_value = excluded.obfuscated_value");
	state->exec(qry, sql, {keychainSecretName, obfuscate(password)});
}

void AccountListModel::deletePasswordFallback(const QString &keychainSecretName)
{
	QSqlQuery qry = m_state.query();
	QString sql = QStringLiteral(
		"delete from insecure_storage where keychain_secret_name = ?");
	m_state.exec(qry, sql, {keychainSecretName});
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
