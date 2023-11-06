// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef DESKTOP_UTILS_ACCOUNTLISTMODEL_H
#define DESKTOP_UTILS_ACCOUNTLISTMODEL_H
#include <QAbstractListModel>
#include <QDateTime>
#include <QMetaType>
#include <QSet>
#include <QUrl>
#include <QVector>

class AvatarListModel;
class QSqlQuery;

namespace utils {
class StateDatabase;
}

class AccountListModel final : public QAbstractListModel {
	Q_OBJECT
public:
	enum class Type : int { Auth = 1, ExtAuth = 2 };

	struct Account {
		Type type;
		QString host;
		QString username;
		QString displayUsername;
		QString avatarFilename;
		QDateTime lastUsed;
	};

	enum Roles {
		TypeRole = Qt::UserRole + 1,
		DisplayUsernameRole,
		AvatarFilenameRole,
	};

	AccountListModel(
		utils::StateDatabase &state, AvatarListModel *avatars,
		QObject *parent = nullptr);
	AccountListModel(const AccountListModel &) = delete;
	AccountListModel(AccountListModel &&) = delete;
	AccountListModel &operator=(const AccountListModel &) = delete;
	AccountListModel &operator=(AccountListModel &&) = delete;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;

	QVariant
	data(const QModelIndex &idx, int role = Qt::DisplayRole) const override;

	bool load(const QUrl &url, const QUrl &extAuthUrl);

	bool isEmpty() const { return m_accounts.isEmpty(); }

	int getMostRecentIndex() const;

	void saveAccount(
		Type type, const QString &displayUsername, const QString &password,
		const QString &avatarFilename, bool insecureFallback);

	void savePassword(
		const QString &password, const QString &keychainSecretName,
		bool insecureFallback);

	void
	readAccountPassword(int jobId, Type type, const QString &displayUsername);

	void readPassword(int jobId, const QString &keychainSecretName);

	void deleteAccountAt(int i);

	void deleteAccountPassword(Type type, const QString &displayUsername);

	void deletePassword(const QString &keychainSecretName);

	static QString buildKeychainSecretName(
		Type type, const QString &host, const QString &username);

	static bool canSavePasswords(bool insecureFallback);

	void clearFallbackPasswords();

signals:
	void passwordReadFinished(int jobId, const QString &password);

private:
	static constexpr char KEYCHAIN_NAME[] = "Drawpile";

	utils::StateDatabase &m_state;
	AvatarListModel *m_avatars;
	QString m_authHost;
	QString m_extAuthHost;
	QVector<Account> m_accounts;

	void createTables();
	void loadAccounts(
		QVector<Account> &accounts, QSet<QString> &usernames, Type type,
		const QString &target);
	QString hostForType(Type type);
	static QString normalizeUsername(const QString &displayUsername);
	static bool isAccountLessThan(const Account &a, const Account &b);

	QString readPasswordFallback(const QString &keychainSecretName);
	static void savePasswordFallback(
		utils::StateDatabase *state, const QString &keychainSecretName,
		const QString &password);
	void deletePasswordFallback(const QString &keychainSecretName);
	static QString obfuscate(const QString &value);
	static QString deobfuscate(const QString &obfuscatedValue);
};

bool operator==(
	const AccountListModel::Account &a, const AccountListModel::Account &b);

Q_DECLARE_METATYPE(AccountListModel::Type)

#endif
