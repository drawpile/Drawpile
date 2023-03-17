/*
   Drawpile - a collaborative drawing program.

   Copyright (C) 2016-2019 Calle Laakkonen

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

#ifndef SERVERCONFIG_H
#define SERVERCONFIG_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QUrl>

class QHostAddress;

namespace server {

class ServerLog;

class ConfigKey {
public:
	enum Type {
		STRING, // A string value
		TIME,   // Time in seconds, converted from a time definition string
		SIZE,   // (File) size in bytes, converted from a size definition string
		INT,    // Integer
		BOOL    // Boolean (true|1)
	};

	ConfigKey() : index(0), name(nullptr), defaultValue(nullptr), type(STRING) { }
	constexpr ConfigKey(int index_, const char *name_, const char *defaultValue_, Type type_)
		: index(index_), name(name_), defaultValue(defaultValue_), type(type_) { }

	const int index;
	const char *name;
	const char *defaultValue;
	const Type type;
};

namespace config {
	static const ConfigKey
		ClientTimeout(0, "clientTimeout", "60", ConfigKey::TIME),            // Connection ping timeout for clients
		SessionSizeLimit(1, "sessionSizeLimit", "99mb", ConfigKey::SIZE),    // Session history size limit in bytes
		SessionCountLimit(2, "sessionCountLimit", "25", ConfigKey::INT),     // Maximum number of active sessions (int)
		EnablePersistence(3, "persistence", "false", ConfigKey::BOOL),       // Enable session persistence (bool)
		AllowGuestHosts(4, "allowGuestHosts", "true", ConfigKey::BOOL),      // Allow guests (or users without the HOST flag) to host sessions
		IdleTimeLimit(5, "idleTimeLimit", "0", ConfigKey::TIME),             // Session idle time limit in seconds (int)
		ServerTitle(6, "serverTitle", "", ConfigKey::STRING),                // Server title (string)
		WelcomeMessage(7, "welcomeMessage", "", ConfigKey::STRING),          // Message sent to a user when they join a session (string)
		AnnounceWhiteList(8, "announceWhitelist", "false", ConfigKey::BOOL), // Should the announcement server whitelist be used (bool)
		PrivateUserList(9, "privateUserList", "false", ConfigKey::BOOL),     // Don't include user list in announcement
		AllowGuests(10, "allowGuests", "true", ConfigKey::BOOL),             // Allow unauthenticated users
		ArchiveMode(11, "archive", "false", ConfigKey::BOOL),                // Don't delete terminated session files
		UseExtAuth(12, "extauth", "false", ConfigKey::BOOL),                 // Enable external authentication
		ExtAuthKey(13, "extauthkey", "", ConfigKey::STRING),                 // ExtAuth signature verification key
		ExtAuthGroup(14, "extauthgroup", "", ConfigKey::STRING),             // ExtAuth user group (leave blank for default set)
		ExtAuthFallback(15, "extauthfallback", "true", ConfigKey::BOOL),     // Fall back to guest logins if ext auth server is unreachable
		ExtAuthMod(16, "extauthmod", "true", ConfigKey::BOOL),               // Respect ext-auth user's "MOD" flag
		ExtAuthHost(17, "extauthhost", "true", ConfigKey::BOOL),             // Respect ext-auth user's "HOST" flag
		AbuseReport(18, "abusereport", "false", ConfigKey::BOOL),            // Enable abuse report (server address must have been set)
		ReportToken(19, "reporttoken", "", ConfigKey::STRING),               // Abuse report backend server authorization token
		LogPurgeDays(20, "logpurgedays", "0", ConfigKey::INT),               // Automatically purge log entries older than this many days (DB log only)
		AutoresetThreshold(21, "autoResetThreshold", "15mb", ConfigKey::SIZE), // Default autoreset threshold in bytes
		AllowCustomAvatars(22, "customAvatars", "true", ConfigKey::BOOL),      // Allow users to set a custom avatar when logging in
		ExtAuthAvatars(23, "extAuthAvatars", "true", ConfigKey::BOOL),         // Use avatars received from ext-auth server (unless a custom avatar has been set)
		ForceNsfm(24, "forceNsfm", "false", ConfigKey::BOOL)                   // Force NSFM flag to be set on all sessions
		;
}

//! Settings that are not adjustable after the server has started
struct InternalConfig {
	QString localHostname; // Hostname of this server to use in session announcements
	int realPort = 27750;  // The port the server is listening on
	int announcePort = 0;  // The port to use in session announcements
	QUrl extAuthUrl;       // URL of the external authentication server
	QUrl reportUrl;        // Abuse report handler backend URL

	int getAnnouncePort() const { return announcePort > 0 ? announcePort : realPort; }
};

struct RegisteredUser {
	enum Status {
		NotFound, // User with this name not found
		BadPass,  // Supplied password did not match
		Banned,   // This username is banned
		Ok        // Can log in
	};

	Status status;
	QString username;
	QStringList flags;
	QString userId;
};

/**
 * @brief Server configuration class
 *
 * These are the configuration settings that can be changed at runtime.
 * The default storage implementation is a simple in-memory key/value map.
 * Deriving classes can implement persistent storage of settings.
 */
class ServerConfig : public QObject
{
	Q_OBJECT
public:
	explicit ServerConfig(QObject *parent=nullptr) : QObject(parent) {}

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
	 * The default implementation sets the value to the in-memory nonpersistent store.
	 *
	 * @param key
	 * @param value
	 * @return false if value didn't validate
	 */
	bool setConfigString(ConfigKey key, const QString &value);

	void setConfigInt(ConfigKey, int value);
	void setConfigBool(ConfigKey, bool value);

	/**
	 * @brief Check if the given listing site URL is allowed
	 *
	 * The default implementation always returns true
	 */
	virtual bool isAllowedAnnouncementUrl(const QUrl &url) const;

	/**
	 * @brief Check if the given address is banned from this server
	 *
	 * The default implementation always returns false
	 */
	virtual bool isAddressBanned(const QHostAddress &addr) const;

	/**
	 * @brief See if there is a registered user with the given credentials
	 *
	 * The default implementation always returns NotFound
	 */
	virtual RegisteredUser getUserAccount(const QString &username, const QString &password) const;

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

signals:
	void configValueChanged(const ConfigKey &key);

protected:
	/**
	 * @brief Get the configuration value for the given key
	 *
	 * The default implementation gets the value from the in-memory nonpersistent
	 * store.
	 * @param key
	 * @param found
	 * @return
	 */
	virtual QString getConfigValue(const ConfigKey key, bool &found) const = 0;
	virtual void setConfigValue(const ConfigKey key, const QString &value) = 0;

private:
	InternalConfig m_internalCfg;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
	class [[maybe_unused]] AbstractServerConfigMarker : ServerConfig
	{
		inline RegisteredUser getUserAccount(const QString &, const QString &) const override { return RegisteredUser(); }
		inline bool isAllowedAnnouncementUrl(const QUrl &) const override { return false; }
		inline bool isAddressBanned(const QHostAddress &) const override { return false; }
	};
}

}

#endif // SERVERCONFIG_H
