// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSERVER_SERVERCONFIG_H
#define LIBSERVER_SERVERCONFIG_H
#include <QDateTime>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QObject>
#include <QString>
#include <QUrl>

class QHostAddress;

namespace server {

class ServerLog;

class ConfigKey {
public:
	enum Type {
		STRING, // A string value
		TIME,	// Time in seconds, converted from a time definition string
		SIZE,	// (File) size in bytes, converted from a size definition string
		INT,	// Integer
		BOOL	// Boolean (true|1)
	};

	ConfigKey()
		: index(0)
		, name(nullptr)
		, defaultValue(nullptr)
		, type(STRING)
	{
	}

	constexpr ConfigKey(
		int index_, const char *name_, const char *defaultValue_, Type type_)
		: index(index_)
		, name(name_)
		, defaultValue(defaultValue_)
		, type(type_)
	{
	}

	const int index;
	const char *name;
	const char *defaultValue;
	const Type type;
};

namespace config {
static const ConfigKey
	// Connection ping timeout for clients
	ClientTimeout(0, "clientTimeout", "60", ConfigKey::TIME),
	// Session history size limit in bytes
	SessionSizeLimit(1, "sessionSizeLimit", "99mb", ConfigKey::SIZE),
	// Maximum number of active sessions (int)
	SessionCountLimit(2, "sessionCountLimit", "25", ConfigKey::INT),
	// Enable session persistence (bool)
	EnablePersistence(3, "persistence", "false", ConfigKey::BOOL),
	// Allow guests (or users without the HOST flag) to host sessions
	AllowGuestHosts(4, "allowGuestHosts", "true", ConfigKey::BOOL),
	// Session idle time limit in seconds (int)
	IdleTimeLimit(5, "idleTimeLimit", "0", ConfigKey::TIME),
	// Server title (string)
	ServerTitle(6, "serverTitle", "", ConfigKey::STRING),
	// Message sent to a user when they join a session (string)
	WelcomeMessage(7, "welcomeMessage", "", ConfigKey::STRING),
	// Should the announcement server whitelist be used (bool)
	AnnounceWhiteList(8, "announceWhitelist", "false", ConfigKey::BOOL),
	// Don't include user list in announcement
	PrivateUserList(9, "privateUserList", "false", ConfigKey::BOOL),
	// Allow unauthenticated users
	AllowGuests(10, "allowGuests", "true", ConfigKey::BOOL),
	// Don't delete terminated session files
	ArchiveMode(11, "archive", "false", ConfigKey::BOOL),
	// Enable external authentication
	UseExtAuth(12, "extauth", "false", ConfigKey::BOOL),
	// ExtAuth signature verification key
	ExtAuthKey(13, "extauthkey", "", ConfigKey::STRING),
	// ExtAuth user group (leave blank for default set)
	ExtAuthGroup(14, "extauthgroup", "", ConfigKey::STRING),
	// Fall back to guest logins if ext auth server is unreachable
	ExtAuthFallback(15, "extauthfallback", "true", ConfigKey::BOOL),
	// Respect ext-auth user's "MOD" flag
	ExtAuthMod(16, "extauthmod", "true", ConfigKey::BOOL),
	// Respect ext-auth user's "HOST" flag
	ExtAuthHost(17, "extauthhost", "true", ConfigKey::BOOL),
	// Enable abuse report (server address must have been set)
	AbuseReport(18, "abusereport", "false", ConfigKey::BOOL),
	// Abuse report backend server authorization token
	ReportToken(19, "reporttoken", "", ConfigKey::STRING),
	// Automatically purge log entries older than this many days (DB log only)
	LogPurgeDays(20, "logpurgedays", "0", ConfigKey::INT),
	// Default autoreset threshold in bytes
	AutoresetThreshold(21, "autoResetThreshold", "15mb", ConfigKey::SIZE),
	// Allow users to set a custom avatar when logging in
	AllowCustomAvatars(22, "customAvatars", "true", ConfigKey::BOOL),
	// Use avatars received from ext-auth server (unless a custom avatar has
	// been set)
	ExtAuthAvatars(23, "extAuthAvatars", "true", ConfigKey::BOOL),
	ForceNsfm(
		24, "forceNsfm", "false",
		ConfigKey::BOOL), // Force NSFM flag to be set on all sessions
	// URL to source an external ban list from.
	ExtBansUrl(25, "extBansUrl", "", ConfigKey::STRING),
	// How often to refresh the external ban list (minimum 1 minute.)
	ExtBansCheckInterval(26, "extBansCheckInterval", "900", ConfigKey::TIME),
	// Last URL used to fetch bans. Internal value used for caching.
	ExtBansCacheUrl(27, "extBansCacheUrl", "", ConfigKey::STRING),
	// Last cache key from external bans. Internal value used for caching.
	ExtBansCacheKey(28, "extBansCacheKey", "", ConfigKey::STRING),
	// Last external bans response. Internal value used for caching.
	ExtBansCacheResponse(29, "extBansCacheResponse", "", ConfigKey::STRING),
	// Respect ext-auth user's "BANEXEMPT" flag.
	ExtAuthBanExempt(30, "extauthbanexempt", "false", ConfigKey::BOOL),
	// Allow mods to disable the idle timer for individual sessions.
	AllowIdleOverride(31, "allowIdleOverride", "true", ConfigKey::BOOL),
	// Url to show users when logging in.
	LoginInfoUrl(32, "loginInfoUrl", "", ConfigKey::STRING),
	// Allow mods to join as ghost users, to avoid disrupting sessions.
	EnableGhosts(33, "enableGhosts", "false", ConfigKey::BOOL),
	// Respect ext-auth user's "GHOST" flag.
	ExtAuthGhosts(34, "extauthghosts", "false", ConfigKey::BOOL),
	// Rules to show to connecting clients.
	RuleText(35, "ruleText", "", ConfigKey::STRING),
	// Minimum required protocol version for hosting sessions.
	MinimumProtocolVersion(36, "minimumProtocolVersion", "", ConfigKey::STRING),
	// Require clients to join sessions through a direct link.
	MandatoryLookup(37, "mandatoryLookup", "false", ConfigKey::BOOL),
	// Allow guests to join via WebSockets.
	AllowGuestWeb(38, "allowGuestWeb", "true", ConfigKey::BOOL),
	// Respect ext-auth user's "WEB" flag.
	ExtAuthWeb(39, "extauthweb", "false", ConfigKey::BOOL),
	// Allow guests to toggle the web setting in sessions.
	AllowGuestWebSession(40, "allowGuestWebSession", "true", ConfigKey::BOOL),
	// Respect ext-auth user's "WEBSESSION" flag.
	ExtAuthWebSession(41, "extauthwebsession", "false", ConfigKey::BOOL),
	// Maximum number of users per session.
	SessionUserLimit(42, "sessionUserLimit", "254", ConfigKey::INT),
	// Automatically allow/disallow web sessions based on passwordedness.
	PasswordDependentWebSession(
		43, "passwordDependentWebSession", "false", ConfigKey::BOOL),
	// Respect ext-auth user's "PERSIST" flag.
	ExtAuthPersist(44, "extauthpersist", "false", ConfigKey::BOOL),
	// How long empty sessions linger after the last user left to maybe give
	// them a chance to reconnect.
	EmptySessionLingerTime(45, "emptySessionLingerTime", "0", ConfigKey::TIME);
}

//! Settings that are not adjustable after the server has started
struct InternalConfig {
	// Hostname of this server to use in session announcements
	QString localHostname;
	// The port the server is listening on
	int realPort = 27750;
	// The port to use in session announcements
	int announcePort = 0;
	// URL of the external authentication server
	QUrl extAuthUrl;
	// Abuse report handler backend URL
	QUrl reportUrl;
	// Key used to encrypt session ban exports
	QByteArray cryptKey;
#ifdef HAVE_WEBSOCKETS
	// Are we listening for WebSocket connections?
	bool webSocket = false;
#endif

	int getAnnouncePort() const
	{
		return announcePort > 0 ? announcePort : realPort;
	}
};

struct RegisteredUser {
	enum Status {
		NotFound, // User with this name not found
		BadPass,  // Supplied password did not match
		Banned,	  // This username is banned
		Ok		  // Can log in
	};

	Status status;
	QString username;
	QStringList flags;
	QString userId;
};

enum class BanReaction {
	NotBanned,
	Unknown,
	NormalBan,
	NetError,
	Garbage,
	Hang,
	Timer,
};

struct BanResult {
	BanReaction reaction;
	QString reason;
	QDateTime expires;
	QString cause;
	QString source;
	QString sourceType;
	int sourceId;
	bool isExemptable;

	static BanResult notBanned()
	{
		return {BanReaction::NotBanned, {}, {}, {}, {}, {}, 0, true};
	}
};

struct BanIpRange {
	QHostAddress from;
	QHostAddress to;
	BanReaction reaction;
};

struct BanSystemIdentifier {
	QSet<QString> sids;
	BanReaction reaction;
};

struct BanUser {
	QSet<long long> ids;
	BanReaction reaction;
};

struct ExtBan {
	int id;
	QVector<BanIpRange> ips;
	QVector<BanIpRange> ipsExcluded;
	QVector<BanSystemIdentifier> system;
	QVector<BanUser> users;
	QDateTime expires;
	QString comment;
	QString reason;
};

/**
 * @brief Server configuration class
 *
 * These are the configuration settings that can be changed at runtime.
 * The default storage implementation is a simple in-memory key/value map.
 * Deriving classes can implement persistent storage of settings.
 */
class ServerConfig : public QObject {
	Q_OBJECT
public:
	explicit ServerConfig(QObject *parent = nullptr)
		: QObject(parent)
	{
	}

	void setInternalConfig(const InternalConfig &cfg) { m_internalCfg = cfg; }
	const InternalConfig &internalConfig() const { return m_internalCfg; }

	// Get configuration values
	QString getConfigString(ConfigKey key) const;
	int getConfigTime(ConfigKey key) const;
	int getConfigSize(ConfigKey key) const;
	int getConfigInt(ConfigKey key) const;
	bool getConfigBool(ConfigKey key) const;
	QVariant getConfigVariant(ConfigKey key) const;

	/**
	 * @brief Set a configuration value
	 * The default implementation sets the value to the in-memory nonpersistent
	 * store.
	 *
	 * @param key
	 * @param value
	 * @return false if value didn't validate
	 */
	bool setConfigString(ConfigKey key, const QString &value);

	void setConfigInt(ConfigKey, int value);
	void setConfigBool(ConfigKey, bool value);

	void setExternalBans(const QVector<ExtBan> &bans) { m_extBans = bans; }
	virtual bool setExternalBanEnabled(int id, bool enabled);
	QJsonArray getExternalBans() const;

	/**
	 * @brief Check if the given listing site URL is allowed
	 *
	 * The default implementation always returns true
	 */
	virtual bool isAllowedAnnouncementUrl(const QUrl &url) const;

	/**
	 * @brief Check if the given address is banned from this server
	 */
	virtual BanResult isAddressBanned(const QHostAddress &addr) const;
	virtual BanResult isSystemBanned(const QString &sid) const;
	virtual BanResult isUserBanned(long long userId) const;

	/**
	 * @brief See if there is a registered user with the given credentials
	 *
	 * The default implementation always returns NotFound
	 */
	virtual RegisteredUser
	getUserAccount(const QString &username, const QString &password) const;

	virtual bool hasAnyUserAccounts() const;

	/**
	 * @brief Get the configured logger instance
	 */
	virtual ServerLog *logger() const = 0;

	/**
	 * @brief Parse a time interval string (e.g. "1d" or "5h")
	 * @param str
	 * @return time in seconds or a negative value in case of error
	 */
	static int parseTimeString(const QString &str);

	/**
	 * @brief Parse a file size string (e.g. "10.3 MB" or "550Kb")
	 * @param str
	 * @return size in bytes or a negative value in case of error
	 */
	static int parseSizeString(const QString &str);

	static QDateTime parseDateTime(const QString &expires);
	static QString formatDateTime(const QDateTime &expires);

	static BanReaction parseReaction(const QString &reaction);

signals:
	void configValueChanged(const ConfigKey &key);

protected:
	const QVector<ExtBan> extBans() const { return m_extBans; }

	/**
	 * @brief Get the configuration value for the given key
	 *
	 * The default implementation gets the value from the in-memory
	 * nonpersistent store.
	 * @param key
	 * @param found
	 * @return
	 */
	virtual QString getConfigValue(const ConfigKey key, bool &found) const = 0;
	virtual void setConfigValue(const ConfigKey key, const QString &value) = 0;

	static bool matchesBannedAddress(
		const QHostAddress &addr, const QHostAddress &ip, int subnet);

	static bool isAddressInRange(
		const QHostAddress &addr, const QHostAddress &from,
		const QHostAddress &to);

	static QString reactionToString(BanReaction reaction);

private:
	static bool isInAnyRange(
		const QHostAddress &addr, const QVector<BanIpRange> &ranges,
		BanReaction *outReaction = nullptr);

	static bool isAddressInRangeV4(
		const QHostAddress &addr, const QHostAddress &from,
		const QHostAddress &to);

	static bool isAddressInRangeV6(
		const QHostAddress &addr, const QHostAddress &from,
		const QHostAddress &to);

	static bool isInAnySystem(
		const QString &sid, const QVector<BanSystemIdentifier> &system,
		BanReaction &outReaction);

	static bool isInAnyUser(
		long long userId, const QVector<BanUser> &users,
		BanReaction &outReaction);

	static BanResult makeBanResult(
		const ExtBan &ban, const QString &cause, const QString &sourceType,
		BanReaction reaction, bool isExemptable);

	static QJsonArray
	banIpRangesToJson(const QVector<BanIpRange> &ranges, bool includeReaction);

	static QJsonArray
	banSystemToJson(const QVector<BanSystemIdentifier> &system);

	static QJsonArray banUsersToJson(const QVector<BanUser> &users);

	InternalConfig m_internalCfg;
	QVector<ExtBan> m_extBans;
	QSet<int> m_disabledExtBanIds;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
class [[maybe_unused]] AbstractServerConfigMarker : ServerConfig {
	inline RegisteredUser
	getUserAccount(const QString &, const QString &) const override
	{
		return RegisteredUser();
	}
	inline bool isAllowedAnnouncementUrl(const QUrl &) const override
	{
		return false;
	}
};
}

}

#endif // SERVERCONFIG_H
