// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSERVER_SERVERLOG_H
#define LIBSERVER_SERVERLOG_H
#include "libshared/util/ulid.h"
#include <QDateTime>
#include <QHostAddress>

class QJsonObject;

namespace server {

class ServerLog;

/**
 * @brief Server log entry
 */
class Log {
	Q_GADGET
public:
	enum class Level {
		Error, // severe message (requires admin attention)
		Warn,  // acceptable errors
		Info,  // useful info for moderators
		Debug, // useful info for developers
	};
	Q_ENUM(Level)

	enum class Topic {
		Join,		// user joined a session
		Leave,		// user left a session
		Kick,		// user was kicked
		Ban,		// user was banned
		Unban,		// a ban was lifted
		Op,			// user was granted OP
		Deop,		// OP status was removed
		Mute,		// User was muted
		Unmute,		// User was unmuted
		Trust,		// User was tagged as trusted
		Untrust,	// User's trusted tag was removed
		BadData,	// Received an invalid message from a client
		RuleBreak,	// User tried to use a command they're not allowed to
		PubList,	// Session announcement
		Status,		// General stuff
		ClientInfo, // Structured information about client joining or hosting
		ExtBan,		// Externally sourced bans
		BanImpEx,	// Session ban import and export
		AdminChat,	// Messages sent to a connected admin chat
		Invite,		// Invite-related events
	};
	Q_ENUM(Topic)

	Log()
		: m_timestamp(QDateTime::currentDateTimeUtc())
		, m_level(Level::Warn)
		, m_topic(Topic::Status)
	{
	}

	Log(const QDateTime &ts, const QString &sessionId, const QString &user,
		Level level, Topic topic, const QString message)
		: m_timestamp(ts)
		, m_session(sessionId)
		, m_user(user)
		, m_level(level)
		, m_topic(topic)
		, m_message(message)
	{
	}

	//! Get the entry timestamp
	QDateTime timestamp() const { return m_timestamp; }

	//! Get the session ID (blank if not pertinent to any session)
	QString session() const { return m_session; }

	//! Get the user info triplet (ID;IP;name) or empty if not pertinent to any
	//! user
	QString user() const { return m_user; }

	//! Get the log entry severity level
	Level level() const { return m_level; }

	//! What's this log entry about?
	Topic topic() const { return m_topic; }

	//! Get the freeform message part
	QString message() const { return m_message; }

	bool isSensitive() const { return m_topic == Topic::ClientInfo; }
	bool isKickOrBan() const
	{
		return m_topic == Topic::Kick || m_topic == Topic::Ban ||
			   m_topic == Topic::Unban;
	}

	Log &about(Level l, Topic t)
	{
		m_level = l;
		m_topic = t;
		return *this;
	}

	Log &user(uint8_t id, const QHostAddress &ip, const QString &name)
	{
		m_user = QStringLiteral("%1;%2;%3")
					 .arg(int(id))
					 .arg(ip.toString())
					 .arg(name);
		return *this;
	}

	Log &session(const QString &id)
	{
		m_session = id;
		return *this;
	}

	Log &message(const QString &msg)
	{
		m_message = msg;
		return *this;
	}

	inline void to(ServerLog *logger);

	/**
	 * @brief Get the log message as a string
	 * @param abridged if true, the timestamp and log level are omitted
	 */
	QString toString(bool abridged = false) const;

	enum JsonOption {
		NoOptions = 0,
		NoPrivateData = 0x01,
		NoSession = 0x02,
	};
	Q_DECLARE_FLAGS(JsonOptions, JsonOption)

	/**
	 * @brief Get the log message as a JSON object
	 *
	 * If noPrivateData is true, this may return a blank object if the whole
	 * log entry contains information only the server administrator should see.
	 *
	 * @param noPrivateData if true, private data (user IP address) is omitted
	 */
	QJsonObject toJson(JsonOptions options = NoOptions) const;

private:
	QDateTime m_timestamp;
	QString m_session;
	QString m_user;
	Level m_level;
	Topic m_topic;
	QString m_message;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Log::JsonOptions)

class ServerLog;

/**
 * @brief Log query builder
 */
class ServerLogQuery {
public:
	ServerLogQuery(const ServerLog &log)
		: m_log(log)
	{
	}

	ServerLogQuery &session(const QString &id)
	{
		m_session = id;
		return *this;
	}

	ServerLogQuery &user(const QString &user)
	{
		m_user = user;
		return *this;
	}

	ServerLogQuery &messageContains(const QString &messageSubstring)
	{
		m_messageSubstring = messageSubstring;
		return *this;
	}

	ServerLogQuery &page(int page, int entriesPerPage)
	{
		m_offset = page * entriesPerPage;
		m_limit = entriesPerPage;
		return *this;
	}

	ServerLogQuery &after(const QDateTime &ts)
	{
		m_after = ts;
		return *this;
	}

	ServerLogQuery &atleast(Log::Level level)
	{
		m_atleast = level;
		return *this;
	}

	ServerLogQuery &omitSensitive(bool omitSensitive)
	{
		m_omitSensitive = omitSensitive;
		return *this;
	}

	ServerLogQuery &omitKicksAndBans(bool omitKicksAndBans)
	{
		m_omitKicksAndBans = omitKicksAndBans;
		return *this;
	}

	QList<Log> get() const;

private:
	const ServerLog &m_log;
	QString m_session;
	QString m_user;
	QString m_messageSubstring;
	int m_offset = 0;
	int m_limit = 0;
	Log::Level m_atleast = Log::Level::Debug;
	QDateTime m_after;
	bool m_omitSensitive = true;
	bool m_omitKicksAndBans = false;
};

/**
 * @brief Abstract base class for server logger implementations
 */
class ServerLog {
public:
	ServerLog()
		: m_silent(false)
	{
	}

	virtual ~ServerLog() = default;

	//! Don't log messages to stderr
	void setSilent(bool silent) { m_silent = silent; }

	/**
	 * @brief Log a message
	 *
	 * @param entry
	 */
	void logMessage(const Log &entry);

	/**
	 * @brief Get all available log messages that match the given filters
	 *
	 * @param session get only log entries for this session
	 * @param user only entries with this user
	 * @param messageSubstring only entries that contain this message substring
	 * @param after get messages whose timestamp is greater than this
	 * @param atleast minimum log level
	 * @param offset ignore first *offset* messages
	 * @param limit return at most this many messages
	 * @param omitSensitive leave out messages with sensitive data, like IPs
	 * @param omitKicksAndBans leave out kick and (un)ban messages from the log
	 */
	virtual QList<Log> getLogEntries(
		const QString &session, const QString &user,
		const QString &messageSubstring, const QDateTime &after,
		Log::Level atleast, bool omitSensitive, bool omitKicksAndBans,
		int offset, int limit) const = 0;

	/**
	 * @brief Return a query builder
	 * @return
	 */
	ServerLogQuery query() const { return ServerLogQuery(*this); }

protected:
	virtual void storeMessage(const Log &entry) = 0;

private:
	bool m_silent;
};

inline QList<Log> ServerLogQuery::get() const
{
	return m_log.getLogEntries(
		m_session, m_user, m_messageSubstring, m_after, m_atleast,
		m_omitSensitive, m_omitKicksAndBans, m_offset, m_limit);
}

void Log::to(ServerLog *logger)
{
	if(logger) {
		logger->logMessage(*this);
	} else {
		qWarning("logger(null): %s", qUtf8Printable(toString()));
	}
}

/**
 * @brief A simple ServerLog implementation that keeps the latest messages in
 * memory
 */
class InMemoryLog : public ServerLog {
public:
	InMemoryLog()
		: m_limit(1000)
	{
	}

	void setHistoryLimit(int limit);

	QList<Log> getLogEntries(
		const QString &session, const QString &user,
		const QString &messageSubstring, const QDateTime &after,
		Log::Level atleast, bool omitSensitive, bool omitKicksAndBans,
		int offset, int limit) const override;

protected:
	void storeMessage(const Log &entry) override;

private:
	QList<Log> m_history;
	int m_limit;
};

}

#endif
