// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef THINSRV_DATABASE_H
#define THINSRV_DATABASE_H
#include "libserver/serverconfig.h"

namespace drawdance {
class Query;
}

namespace server {

class ExtBans;

class Database final : public ServerConfig {
	Q_OBJECT
public:
	explicit Database(QObject *parent = nullptr);
	~Database() override;

	Q_INVOKABLE bool openFile(const QString &path);

	void loadExternalIpBans(ExtBans *extBans);

	bool setExternalBanEnabled(int id, bool enabled) override;

	bool isAllowedAnnouncementUrl(const QUrl &url) const override;
	BanResult isAddressBanned(const QHostAddress &addr) const override;
	BanResult isSystemBanned(const QString &sid) const override;
	BanResult isUserBanned(long long userId) const override;
	RegisteredUser getUserAccount(
		const QString &username, const QString &password) const override;
	bool hasAnyUserAccounts() const override;
	bool supportsAdminSectionLocks() const override;
	bool isAdminSectionLocked(const QString &section) const override;
	bool checkAdminSectionLockPassword(const QString &password) const override;
	bool setAdminSectionsLocked(
		const QSet<QString> &sections, const QString &password) override;
	ServerLog *logger() const override;

	//! Get the list server URL whitelist
	QStringList listServerWhitelist() const;

	//! Replace the list server URL whitelist
	void updateListServerWhitelist(const QStringList &whitelist);

	//! Get a JSON representation of the full banlist
	QJsonArray getIpBanlist() const;
	QJsonArray getSystemBanlist() const;
	QJsonArray getUserBanlist() const;
	QJsonObject addIpBan(
		const QHostAddress &ip, int subnet, const QDateTime &expiration,
		const QString &comment);
	QJsonObject addSystemBan(
		const QString &sid, const QDateTime &expires, BanReaction reaction,
		const QString &reason, const QString &comment);
	QJsonObject addUserBan(
		long long userId, const QDateTime &exires, BanReaction reaction,
		const QString &reason, const QString &comment);
	bool deleteIpBan(int entryId);
	bool deleteSystemBan(int entryId);
	bool deleteUserBan(int entryId);

	//! Get a JSON representation of registered user accounts
	QJsonArray getAccountList() const;
	QJsonObject addAccount(
		const QString &username, const QString &password, bool locked,
		const QStringList &flags);
	QJsonObject updateAccount(int id, const QJsonObject &update);
	bool deleteAccount(int id);

private slots:
	void dailyTasks();

protected:
	QString getConfigValue(const ConfigKey key, bool &found) const override;
	void setConfigValue(ConfigKey key, const QString &value) override;

private:
	QString getConfigValueByName(const QString &name, bool &found) const;
	void setConfigValueByName(const QString &name, const QString &value);

	static QJsonObject ipBanResultToJson(const drawdance::Query &query);
	static QJsonObject systemBanResultToJson(const drawdance::Query &query);
	static QJsonObject userBanResultToJson(const drawdance::Query &query);

	struct Private;
	Private *d;
};

}

#endif // DATABASE_H
