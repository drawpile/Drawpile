// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_SERVER_SESSION_H
#define LIBSHARED_SERVER_SESSION_H
#include "libserver/announcable.h"
#include "libserver/jsonapi.h"
#include "libserver/sessionhistory.h"
#include "libshared/net/message.h"
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QTimer;
struct DP_Recorder;

namespace sessionlisting {
class Announcements;
}

namespace server {

class Client;
class ConfigKey;
class ServerConfig;
class Log;

/**
 * The serverside session state.
 *
 * This is an abstract base class. Concrete implementations are ThinSession and
 * ThickSession, for thin and thick servers respectively.
 */
class Session : public QObject, public sessionlisting::Announcable {
	Q_OBJECT
public:
	//! State of the session
	enum class State { Initialization, Running, Reset, Shutdown };

	enum class ResetCapability { GzipStream = 1 << 0 };
	Q_DECLARE_FLAGS(ResetCapabilities, ResetCapability)

	//! Information about a user who has since logged out
	struct PastClient {
		int id;
		QString authId;
		QString username;
		QHostAddress peerAddress;
		QString sid;
		bool isBannable;
	};

	struct AutoResetResponseParams {
		int ctxId;
		ResetCapabilities capabilities;
		int osQuality;
		qreal netQuality;
		qreal averagePing;
	};

	~Session() override;

	//! Get the server configuration
	const ServerConfig *config() const { return m_config; }

	//! Get the unique ID of the session
	QString id() const override final { return m_history->id(); }

	/**
	 * @brief Get the custom alias for the session ID
	 *
	 * Session ID alias is optional. If set, it can be used in place of the ID
	 * when joining a session.
	 */
	QString idAlias() const { return m_history->idAlias(); }

	//! Get the session alias if set, or the ID if not
	QString aliasOrId() const
	{
		return m_history->idAlias().isEmpty() ? id() : m_history->idAlias();
	}

	/**
	 * @brief Set the name of the recording file to create
	 *
	 * Recording will start immediately if the session is in a running state and
	 * no other recording is already running. Otherwise it will start after the
	 * next reset.
	 *
	 * @param filename path to output file
	 */
	void setRecordingFile(const QString &filename);

	/**
	 * @brief Is the session closed to new users?
	 *
	 * A session can be closed in two ways:
	 * - by setting the Closed flag
	 * - when user count reaches maxUsers()
	 *
	 * The session is also temporarily closed during the Reset state.
	 *
	 * @param ignoreFlag Whether to ignore the closed flag
	 * @return true if new users will not be admitted
	 */
	bool isClosed(bool ignoreFlag = false) const;
	void setClosed(bool closed);

	/**
	 *  Does this session support autoresetting?
	 */
	virtual bool supportsAutoReset() const = 0;

	//! Set session attributes
	void setSessionConfig(const QJsonObject &conf, Client *changedBy);

	/**
	 * @brief Add a new client to the session
	 * @param user the client to add
	 * @param host is this the hosting user
	 * @param invite the invite used, if any
	 */
	void joinUser(Client *user, bool host, const Invite *invite = nullptr);

	/**
	 * @brief Assign an ID for this user
	 *
	 * This is used during the login phase to prepare
	 * the user for joining the session
	 * @param user
	 */
	void assignId(Client *user);

	/**
	 * @brief Get a client by ID
	 * @param id user ID
	 * @return user or nullptr if not found
	 */
	Client *getClientById(uint8_t id);
	Client *getClientByAuthId(const QString &authId);

	//! Has a client with the given ID been logged in (not currently)?
	bool hasPastClientWithId(uint8_t id) const
	{
		return m_pastClients.contains(id);
	}

	//! Get information about a past client who used the given ID
	PastClient getPastClientById(uint8_t id) const
	{
		Q_ASSERT(hasPastClientWithId(id));
		return m_pastClients[id];
	}

	/**
	 * @brief Get a client by user name
	 *
	 * The name comparison is case insensitive.
	 * Note! In debug mode username uniqueness is not enforced!
	 * @param username
	 * @return client or nullptr if not found
	 */
	Client *getClientByUsername(const QString &username);

	/**
	 * @brief Add an in-session IP ban for the given client
	 */
	bool addBan(
		const Client *target, const QString &bannedBy,
		const Client *client = nullptr);

	bool addPastBan(
		const PastClient &target, const QString &bannedBy,
		const Client *client = nullptr);

	bool isBanned(const Client *target) const;
	bool isPastBanned(const PastClient &target) const;

	/**
	 * @brief Remove a session specific IP ban
	 * @param entryId ban entry ID
	 */
	void removeBan(int entryId, const QString &removedBy);

	//! Get the number of connected clients
	int userCount() const { return m_clients.size(); }

	//! Get the number of clients that were actively drawing in the given time.
	int activeDrawingUserCount(qint64 ms) const;

	//! A session is empty if it contains no non-ghost users.
	bool isEffectivelyEmpty() const;

	//! Get the of clients currently in this session
	const QVector<Client *> &clients() const { return m_clients; }

	/**
	 * @brief Get the ID of the user uploading initialization or reset data
	 * @return user ID or -1 if init not in progress
	 */
	int initUserId() const { return m_initUser; }

	//! Get the names of this session's users
	QStringList userNames() const;

	/**
	 * @brief Get session uptime in nice human readable format
	 * @return
	 */
	QString uptime() const;

	/**
	 * @brief Get the time of the last join/logout event
	 * @return milliseconds since last event
	 */
	qint64 lastEventTime() const { return m_lastEventTime.elapsed(); }

	//! Get the session history
	const SessionHistory *history() const { return m_history; }
	SessionHistory *history() { return m_history; }

	/**
	 * @brief Process a message received from a client
	 * @param client
	 * @param message
	 */
	void handleClientMessage(Client &client, const net::Message &message);

	/**
	 * @brief Initiate the shutdown of this session
	 *
	 * If the terminate parameter is false, the session history will not be
	 * terminated. This allows the session to survive server restarts. If quiet
	 * is true, the users will not be informed of the termination.
	 */
	void killSession(
		const QString &message, bool terminate = true, bool quiet = false);

	/**
	 * @brief Send a direct message to all session participants
	 *
	 * This bypasses the session history.
	 * @param msg
	 */
	void directToAll(const net::Message &msg);

	bool messageAll(const QString &message, bool alert);

	bool keyMessageAll(
		const QString &message, bool alert, const QString &key,
		const QJsonObject &params = {});

	/**
	 * @brief Generate a request for session announcement
	 *
	 * @param url listing server API url
	 * @return If the session was listable (but not if the listing succeeded)
	 */
	bool makeAnnouncement(const QUrl &url);

	/**
	 * @brief Generate a request for session announcement unlisting
	 * @param url API url
	 * @param terminate if false, the removal is not logged in the history
	 * journal
	 * @param removeOnly if true, an unlisting request is not sent (use in case
	 * of error)
	 */
	void unlistAnnouncement(const QUrl &url, bool terminate = true);

	sessionlisting::Session getSessionAnnouncement() const override;

	bool hasUrgentAnnouncementChange(
		const sessionlisting::Session &description) const override;

	void sendListserverMessage(const QString &message) override
	{
		messageAll(message, false);
	}

	//! Get the session state
	State state() const { return m_state; }

	// Resetting related functions, called via opcommands
	void resetSession(int resetter, const QString &type);
	virtual void readyToAutoReset(
		const AutoResetResponseParams &params, const QString &payload) = 0;
	void handleInitBegin(int ctxId);
	void handleInitComplete(int ctxId);
	void handleInitCancel(int ctxId);

	virtual StreamResetStartResult
	handleStreamResetStart(int ctxId, const QString &correlator) = 0;

	virtual StreamResetAbortResult handleStreamResetAbort(int ctxId) = 0;

	virtual StreamResetPrepareResult
	handleStreamResetFinish(int ctxId, int expectedMessageCount) = 0;

	Invite *createInvite(Client *creator, int maxUses, bool trust, bool op);
	bool removeInvite(Client *client, const QString &secret);
	bool hasInvite(const QString &secret) const;
	CheckInviteResult checkInvite(
		Client *client, const QString &secret, bool use,
		Invite **outInvite = nullptr);

	/**
	 * @brief Grant or revoke OP status of a user
	 * @param id user ID
	 * @param op new status
	 * @param changedBy name of the user who issued the command
	 * @param sendUpdate whether to send an updated auth list to operators
	 * @param invite the invite that triggered this change, if any
	 */
	void changeOpStatus(
		uint8_t id, bool op, const QString &changedBy, bool sendUpdate = true,
		const Invite *invite = nullptr);

	/**
	 * @brief Grant or revoke trusted status of a user
	 * @param id user ID
	 * @param trusted new status
	 * @param changedBy name of the user who issued the command
	 * @param sendUpdate whether to send an updated auth list to operators
	 * @param invite the invite that triggered this change, if any
	 */
	void changeTrustedStatus(
		uint8_t id, bool trusted, const QString &changedBy,
		bool sendUpdate = true, const Invite *invite = nullptr);

	//! Send refreshed ban list to all logged in users
	void sendUpdatedBanlist(bool clearForNonOperators = false);

	//! Send refreshed session announcement list to all logged in users
	void sendUpdatedAnnouncementList();

	//! Send a refreshed list of muted users
	void sendUpdatedMuteList();

	//! Send a refreshed list or registered users with op and trusted states.
	void sendUpdatedAuthList();

	void sendUpdatedInviteList();

	/**
	 * @brief Send an abuse report
	 *
	 * A reporting backend server must have been configured.
	 *
	 * @param reporter the user who is making the report
	 * @param aboutUser the ID of the user this report is about (if 0, the
	 * report is about the session in general)
	 * @param message freeform message entered by the reporter
	 */
	void sendAbuseReport(
		const Client *reporter, int aboutUser, const QString &message);

	QString
	startThumbnailGeneration(const Client *client, const QString &reason);
	bool cancelThumbnailGeneration(const Client *client);
	void handleThumbnailError(
		const Client *client, const QString &correlator, const QString &error);

	/**
	 * @brief Get a JSON object describing the session
	 *
	 * This is used in the login phase session list
	 * and the JSON api.
	 *
	 * @param full - include detailed information (for admin use)
	 * @param invite - adjust description for an explicit invite
	 * @return
	 */
	virtual QJsonObject
	getDescription(bool full = false, bool invite = false) const;

	QJsonObject getExportBanList() const;

	bool importBans(
		const QJsonObject &data, int &outTotal, int &outImported,
		const Client *client = nullptr);

	/**
	 * @brief Call the server's JSON administration API
	 *
	 * This is used by the HTTP admin API.
	 *
	 * @param method query method
	 * @param path path components
	 * @param request request body content
	 * @param sectionLocked if this section is locked for writing
	 * @return JSON API response content
	 */
	JsonApiResult callJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request, bool sectionLocked);

	/**
	 * @brief Write a session related log entry.
	 * The abridged version is also sent to all active memeers of the session.
	 */
	Q_SLOT void log(const Log &entry);

	// Log a message for the given client if non-null, otherwise a session log.
	void logFor(Client *client, const Log &entry);

signals:
	/**
	 * @brief A publishable session attribute just changed.
	 *
	 * This signal is emitted when any of the following attributes are changed:
	 *
	 * - title
	 * - open/closed status
	 * - maximum user count
	 * - password
	 * - persistent mode
	 * - number of users
	 *
	 * @param thisSession
	 */
	void sessionAttributeChanged(Session *thisSession);

	void sessionDestroyed(Session *thisSession);

private slots:
	void removeUser(Client *user);
	void onAnnouncementsChanged(const Announcable *session);
	void
	onAnnouncementError(const Announcable *session, const QString &message);
	void onConfigValueChanged(const ConfigKey &key);

protected:
	Session(
		SessionHistory *history, ServerConfig *config,
		sessionlisting::Announcements *announcements, QObject *parent);

	//! Add a message to the session history
	virtual void addToHistory(const net::Message &msg) = 0;

	//! Session history was just initialized after hosting
	virtual void onSessionInitialized() = 0;

	//! Session history was just reset
	virtual void onSessionReset() = 0;

	//! A regular (non-hosting) client just joined
	virtual void onClientJoin(Client *client, bool host) = 0;

	//! A client just left, clean up reset states and similar
	virtual void onClientLeave(Client *client);

	//! Client has just been deopped, cancel streamed resets and such
	virtual void onClientDeop(Client *client) = 0;

	//! A streamed reset message was received
	virtual void onResetStream(Client &client, const net::Message &msg) = 0;

	//! This message was just added to session history
	void addedToHistory(const net::Message &msg);

	void switchState(State newstate);

	virtual void onStateChanged() = 0;

	virtual void chatMessageToAll(const net::Message &msg);

	//! Get the user join, SessionOwner, etc. messages that should be prepended
	//! to a reset image
	net::MessageList serverSideStateMessages() const;

	void sendUpdatedSessionProperties();

private:
	class AdminChat;
	friend class AdminChat;

	// If someone sent a drawing command in the last 5 minutes, they are active.
	static constexpr qint64 ACTIVE_THRESHOLD_MS = 5 * 60 * 1000;

	void addClientMessage(const Client &client, const net::Message &msg);

	/**
	 * Add a message to the initialization stream
	 *
	 * During init state, this goes to the normal command stream.
	 * During reset state, this goes to a reset buffer which will
	 * replace the old session history once completed.
	 */
	void addToInitStream(const net::Message &msg);

	/**
	 * @brief Update session operator bits
	 *
	 * Generates log entries for each change
	 *
	 * @param ids new list of session operators
	 * @param changedBy name of the user who issued the change command
	 * @param sendUpdate whether to send an updated auth list to operators
	 * @param invite the invite that triggered this change, if any
	 * @return sanitized list of actual session operators
	 */
	QVector<uint8_t> updateOwnership(
		QVector<uint8_t> ids, const QString &changedBy, bool sendUpdate = true,
		const Invite *invite = nullptr);

	/**
	 * @brief Update the list of trusted users
	 *
	 * Generates log entries for each change
	 * @param ids new list of trusted users
	 * @param changedBy name of the user who issued the change command
	 * @param sendUpdate whether to send an updated auth list to operators
	 * @param invite the invite that triggered this change, if any
	 * @return sanitized list of actual trusted users
	 */
	QVector<uint8_t> updateTrustedUsers(
		QVector<uint8_t> ids, const QString &changedBy, bool sendUpdate = true,
		const Invite *invite = nullptr);

	void restartRecording();
	void stopRecording();
	void abortReset();

	void ensureOperatorExists();

	void kickWebUsers(Client *by);

	JsonApiResult callListingsJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	JsonApiResult callAuthJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	JsonApiResult callChatJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	JsonApiResult callInvitesJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	JsonApiResult callThumbnailJsonApi(
		JsonApiMethod method, const QStringList &path,
		const QJsonObject &request);

	void timeOutAdminChat();

	QJsonArray getListingsDescription() const;
	QJsonObject getUserDescription(const Client *user) const;
	QJsonArray getInvitesDescription(bool full = false) const;

	SessionHistory *m_history;
	ServerConfig *m_config;
	sessionlisting::Announcements *m_announcements;
	AdminChat *m_adminChat = nullptr;

	State m_state = State::Initialization;
	int m_initUser = -1; // the user who is currently uploading init/reset data

	DP_Recorder *m_recorder = nullptr;
	QString m_recordingFile;

	QVector<Client *> m_clients;
	QHash<int, PastClient> m_pastClients;

	net::MessageList m_resetstream;
	uint m_resetstreamsize = 0;

	QElapsedTimer m_lastEventTime;

	bool m_closed = false;
};

// https://gcc.gnu.org/bugzilla/show_bug.cgi?id=69210
namespace diagnostic_marker_private {
class [[maybe_unused]] AbstractSessionMarker : Session {};
}

Q_DECLARE_OPERATORS_FOR_FLAGS(Session::ResetCapabilities)

}

#endif
