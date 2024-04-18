// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef LIBSHARED_NET_SERVERCMD_H
#define LIBSHARED_NET_SERVERCMD_H
#include "libshared/net/message.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace net {

/**
 * @brief A command sent to the server using the (Control) Command message
 */
struct ServerCommand {
	QString cmd;
	QJsonArray args;
	QJsonObject kwargs;

	static ServerCommand fromMessage(const net::Message &msg);
	static ServerCommand fromJson(const QJsonDocument &doc);
	net::Message toMessage() const;

	// Convenience functions_
	static net::Message make(
		const QString &cmd, const QJsonArray &args = QJsonArray(),
		const QJsonObject &kwargs = QJsonObject());

	//! Kick (and optionally ban) a user from the session
	static net::Message makeKick(int target, bool ban);

	//! Remove a ban entry
	static net::Message makeUnban(int entryId);

	//! (Un)mute a user
	static net::Message makeMute(int target, bool mute);

	//! Request the server to announce this session at a listing server
	static net::Message makeAnnounce(const QString &url);

	//! Request the server to remove an announcement at the listing server
	static net::Message makeUnannounce(const QString &url);
};

/**
 * @brief A reply or notification from the server received with the Command
 * message
 */
struct ServerReply {
	static constexpr char KEY_BANEXPORT_MODONLY[] = "banexportmodonly";
	static constexpr char KEY_BANEXPORT_SERVERERROR[] = "banexportservererror";
	static constexpr char KEY_BANEXPORT_UNCONFIGURED[] =
		"banexportunconfigured";
	static constexpr char KEY_BANEXPORT_UNSUPPORTED[] = "banexportunsupported";
	static constexpr char KEY_BANIMPORT_CRYPTERROR[] = "banimportcrypterror";
	static constexpr char KEY_BANIMPORT_INVALID[] = "banimportinvalid";
	static constexpr char KEY_BANIMPORT_MALFORMED[] = "banimportmalformed";
	static constexpr char KEY_BANIMPORT_UNCONFIGURED[] =
		"banimportunconfigured";
	static constexpr char KEY_BANIMPORT_UNSUPPORTED[] = "banimportunsupported";
	static constexpr char KEY_BAN[] = "ban";
	static constexpr char KEY_KICK[] = "kick";
	static constexpr char KEY_OUT_OF_SPACE[] = "outofspace";
	static constexpr char KEY_OP_GIVE[] = "opgive";
	static constexpr char KEY_OP_TAKE[] = "optake";
	static constexpr char KEY_RESET_CANCEL[] = "resetcancel";
	static constexpr char KEY_RESET_FAILED[] = "resetfailed";
	static constexpr char KEY_RESET_PREPARE[] = "resetprepare";
	static constexpr char KEY_TERMINATE_SESSION[] = "terminatesession";
	static constexpr char KEY_TERMINATE_SESSION_ADMIN[] =
		"terminatesessionadmin";
	static constexpr char KEY_TERMINATE_SESSION_REASON[] =
		"terminatesessionreason";
	static constexpr char KEY_TRUST_GIVE[] = "trustgive";
	static constexpr char KEY_TRUST_TAKE[] = "trusttake";

	enum class ReplyType {
		Unknown,
		Login,			  // used during the login phase
		Message,		  // general chat type notifcation message
		Alert,			  // urgen notification message
		Error,			  // error occurred
		Result,			  // command result
		Log,			  // server log message
		SessionConf,	  // session configuration update
		SizeLimitWarning, // session history size nearing limit (deprecated)
		Status,			  // periodic status update
		Reset,			  // session reset state
		Catchup,		  // number of messages queued for upload
		ResetRequest,	  // request client to perform a reset
		CaughtUp,		  // previous catchup is complete
		BanImpEx,		  // session ban import/export
		OutOfSpace,		  // session is out of space, block local drawing
	} type;
	QString message;
	QJsonObject reply;

	static ServerReply fromMessage(const net::Message &msg);
	static ServerReply fromJson(const QJsonDocument &doc);

	static net::Message make(const QJsonObject &data);

	static net::Message makeError(const QString &message, const QString &code);

	static net::Message
	makeCommandError(const QString &command, const QString &message);

	static net::Message makeBanExportResult(const QString &data);

	static net::Message makeBanImportResult(int total, int imported);

	static net::Message
	makeBanImpExError(const QString &message, const QString &key);

	static net::Message makeMessage(const QString &message);

	static net::Message makeAlert(const QString &message);

	// Translatable messages. If the client knows the key, it will use a
	// translated message. Otherwise, it will just print the literal message.

	static net::Message makeKeyMessage(
		const QString &message, const QString &key,
		const QJsonObject &params = {});

	static net::Message makeKeyAlert(
		const QString &message, const QString &key,
		const QJsonObject &params = {});

	// They key is used for correlation in thin sessions, where caughtup
	// messages are stored in the session history. Key 0 stands for a reset, -1
	// means no key, other keys are for joining clients catching up.
	static net::Message makeCatchup(int count, int key);
	static net::Message makeCaughtUp(int key);

	static net::Message makeLog(const QString &message, QJsonObject data);

	static net::Message makeLoginGreeting(
		const QString &message, int version, const QJsonArray &flags,
		const QJsonObject &methods, const QString &info, const QString &rules);

	static net::Message makeLoginWelcome(
		const QString &message, const QString &title,
		const QJsonArray &sessions);

	static net::Message
	makeLoginTitle(const QString &message, const QString &title);

	static net::Message
	makeLoginSessions(const QString &message, const QJsonArray &sessions);

	static net::Message
	makeLoginRemoveSessions(const QString &message, const QJsonArray &remove);

	static net::Message makeReset(const QString &message, const QString &state);

	static net::Message makeResetRequest(int maxSize, bool query);

	static net::Message makeResultHostLookup(const QString &message);

	static net::Message
	makeResultJoinLookup(const QString &message, const QJsonObject &session);

	static net::Message
	makeResultPasswordNeeded(const QString &message, const QString &state);

	static net::Message makeResultLoginOk(
		const QString &message, const QString &state, const QJsonArray &flags,
		const QString &ident, bool guest);

	static net::Message makeResultExtAuthNeeded(
		const QString &message, const QString &state, const QString &extauthurl,
		const QString &nonce, const QString &group, bool avatar);

	static net::Message makeResultJoinHost(
		const QString &message, const QString &state, const QJsonObject &join);

	static net::Message
	makeResultStartTls(const QString &message, bool startTls);

	static net::Message makeResultIdentIntentMismatch(
		const QString &message, const QString &intent, const QString &method,
		bool extAuthFallback);

	static net::Message makeResultGarbage();

	static net::Message makeSessionConf(const QJsonObject &config);

	static net::Message makeSizeLimitWarning(int size, int maxSize);

	static net::Message makeOutOfSpace();

	static net::Message makeStatusUpdate(int size);
};

}

#endif
