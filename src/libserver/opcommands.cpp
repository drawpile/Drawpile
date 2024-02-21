// SPDX-License-Identifier: GPL-3.0-or-later
#include "libserver/opcommands.h"
#include "libserver/client.h"
#include "libserver/serverconfig.h"
#include "libserver/serverlog.h"
#include "libserver/session.h"
#include "libshared/net/servercmd.h"
#include "libshared/util/passwordhash.h"
#include <QJsonArray>
#include <QList>
#include <QRegularExpression>
#include <QStringList>
#include <QUrl>
#include <utility>
#ifdef HAVE_LIBSODIUM
#	include <sodium.h>
#endif

namespace server {

namespace {

class CmdResult {
public:
	static CmdResult ok()
	{
		return CmdResult(true, QString(), net::Message::null());
	}

	static CmdResult err(const QString &message)
	{
		return CmdResult(false, message, net::Message::null());
	}

	static CmdResult reply(net::Message &&reply)
	{
		return CmdResult(false, QString(), std::forward<net::Message>(reply));
	}

	bool success() const { return m_success; }

	net::Message getReply(const QString &command) const
	{
		if(m_reply.isNull()) {
			return net::ServerReply::makeCommandError(command, m_message);
		} else {
			return m_reply;
		}
	}

private:
	CmdResult(bool success, const QString &message, net::Message &&reply)
		: m_success(success)
		, m_message(message)
		, m_reply(reply)
	{
	}

	bool m_success;
	QString m_message;
	net::Message m_reply;
};

typedef CmdResult (*SrvCommandFn)(
	Client *, const QJsonArray &, const QJsonObject &);

class SrvCommand {
public:
	enum Mode {
		NONOP,	// usable by all
		DEPUTY, // needs at least deputy privileges
		OP,		// needs operator privileges
		MOD		// needs moderator privileges
	};

	SrvCommand(const QString &name, SrvCommandFn fn, Mode mode = OP)
		: m_fn(fn)
		, m_name(name)
		, m_mode(mode)
	{
	}

	CmdResult
	call(Client *c, const QJsonArray &args, const QJsonObject &kwargs) const
	{
		return m_fn(c, args, kwargs);
	}

	const QString &name() const { return m_name; }

	Mode mode() const { return m_mode; }

private:
	SrvCommandFn m_fn;
	QString m_name;
	Mode m_mode;
};

struct SrvCommandSet {
	QList<SrvCommand> commands;

	SrvCommandSet();
};

const SrvCommandSet COMMANDS;

CmdResult readyToAutoReset(
	Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->readyToAutoReset(client->id());
	return CmdResult::ok();
}

CmdResult
initBegin(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitBegin(client->id());
	return CmdResult::ok();
}

CmdResult
initComplete(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitComplete(client->id());
	return CmdResult::ok();
}

CmdResult
initCancel(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);
	client->session()->handleInitCancel(client->id());
	return CmdResult::ok();
}

CmdResult
sessionConf(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	client->session()->setSessionConfig(kwargs, client);
	return CmdResult::ok();
}

CmdResult
opWord(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: opword");

	const QByteArray opwordHash = client->session()->history()->opwordHash();
	if(opwordHash.isEmpty())
		return CmdResult::err("No opword set");

	if(passwordhash::check(args.at(0).toString(), opwordHash)) {
		client->session()->changeOpStatus(client->id(), true, "password");
		return CmdResult::ok();

	} else {
		return CmdResult::err("Incorrect password");
	}
}

static CmdResult
getClient(Session *session, const QJsonValue &idOrName, Client *&outClient)
{
	Client *c;
	if(idOrName.isDouble()) {
		// ID number
		const int id = idOrName.toInt();
		if(id < 1 || id > 254)
			return CmdResult::err("invalid user id: " + QString::number(id));
		c = session->getClientById(id);

	} else if(idOrName.isString()) {
		// Username
		c = session->getClientByUsername(idOrName.toString());
	} else {
		return CmdResult::err("invalid user ID or name");
	}

	if(c) {
		outClient = c;
		return CmdResult::ok();
	} else {
		return CmdResult::err("user not found");
	}
}

CmdResult
kickUser(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: user ID or name");

	const bool ban = kwargs["ban"].toBool();

	if(ban && client->session()->hasPastClientWithId(args.at(0).toInt())) {
		// Retroactive ban
		const auto target =
			client->session()->getPastClientById(args.at(0).toInt());
		if(target.isBannable) {
			client->session()->addPastBan(target, client->username(), client);
			client->session()->keyMessageAll(
				target.username + " banned by " + client->username(), false,
				net::ServerReply::KEY_BAN,
				{{QStringLiteral("target"), target.username},
				 {QStringLiteral("by"), client->username()}});
			return CmdResult::ok();
		} else {
			return CmdResult::err(target.username + " cannot be banned.");
		}
	}

	Client *target;
	CmdResult result = getClient(client->session(), args.at(0), target);
	if(!result.success()) {
		return result;
	}

	if(target == client)
		return CmdResult::err("cannot kick self");

	if(target->isModerator())
		return CmdResult::err("cannot kick moderators");

	if(client->isDeputy()) {
		if(target->isOperator() || target->isTrusted())
			return CmdResult::err("cannot kick trusted users");
	}

	if(ban) {
		client->session()->keyMessageAll(
			target->username() + " banned by " + client->username(), false,
			net::ServerReply::KEY_BAN,
			{{QStringLiteral("target"), target->username()},
			 {QStringLiteral("by"), client->username()}});
		target->disconnectClient(
			Client::DisconnectionReason::Kick, client->username(),
			QStringLiteral("via op command"));
		client->session()->addBan(target, client->username(), client);
	} else {
		target->disconnectClient(
			Client::DisconnectionReason::Kick, client->username(),
			QStringLiteral("via op command"));
		client->session()->keyMessageAll(
			target->username() + " kicked by " + client->username(), false,
			net::ServerReply::KEY_KICK,
			{{QStringLiteral("target"), target->username()},
			 {QStringLiteral("by"), client->username()}});
	}

	return CmdResult::ok();
}

CmdResult
removeBan(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: ban entry ID");

	client->session()->removeBan(args.at(0).toInt(), client->username());
	return CmdResult::ok();
}

CmdResult
killSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	QString reason = kwargs[QStringLiteral("reason")].toString().trimmed();
	if(reason.isEmpty()) {
		client->session()->keyMessageAll(
			QStringLiteral("Session shut down by moderator (%1)")
				.arg(client->username()),
			true, net::ServerReply::KEY_TERMINATE_SESSION,
			{{QStringLiteral("by"), client->username()}});
	} else {
		client->session()->keyMessageAll(
			QStringLiteral("Session shut down by moderator (%1): %2")
				.arg(client->username(), reason),
			true, net::ServerReply::KEY_TERMINATE_SESSION_REASON,
			{{QStringLiteral("by"), client->username()},
			 {QStringLiteral("reason"), reason}});
	}
	client->session()->killSession(
		QStringLiteral("Session terminated by moderator"));
	return CmdResult::ok();
}

CmdResult announceSession(
	Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: API URL");

	QUrl apiUrl{args.at(0).toString()};
	if(!apiUrl.isValid())
		return CmdResult::err("Invalid API URL");

	client->session()->makeAnnouncement(apiUrl, kwargs["private"].toBool());
	return CmdResult::ok();
}

CmdResult
unlistSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1)
		return CmdResult::err("Expected one argument: API URL");

	client->session()->unlistAnnouncement(args.at(0).toString());
	return CmdResult::ok();
}

CmdResult
resetSession(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	Q_UNUSED(kwargs);

	if(client->session()->state() != Session::State::Running)
		return CmdResult::err("Unable to reset in this state");

	client->session()->resetSession(client->id());
	return CmdResult::ok();
}

CmdResult
setMute(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);

	if(args.size() != 2)
		return CmdResult::err("Expected two arguments: userId true/false");

	Client *target;
	CmdResult result = getClient(client->session(), args.at(0), target);
	if(!result.success()) {
		return result;
	}

	const bool m = args.at(1).toBool();
	if(m && target->isModerator()) {
		return CmdResult::err("cannot mute moderators");
	}

	if(target->isMuted() != m) {
		target->setMuted(m);
		client->session()->sendUpdatedMuteList();
		if(m)
			target->log(Log()
							.about(Log::Level::Info, Log::Topic::Mute)
							.message("Muted by " + client->username()));
		else
			target->log(Log()
							.about(Log::Level::Info, Log::Topic::Unmute)
							.message("Unmuted by " + client->username()));
	}
	return CmdResult::ok();
}

CmdResult
reportAbuse(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);

	const int user = kwargs["user"].toInt();
	const QString reason = kwargs["reason"].toString();

	client->session()->sendAbuseReport(client, user, reason);
	return CmdResult::ok();
}

QString buildBanExport(
	Client *client, ServerLog *logger, QByteArray cryptKey, QByteArray input)
{
	QByteArray compressed = qCompress(input);
	if(compressed.isEmpty()) {
		logger->logMessage(
			Log()
				.user(client->id(), client->peerAddress(), client->username())
				.about(Log::Level::Error, Log::Topic::BanImpEx)
				.message(
					QStringLiteral("Error compressing ban list from session %1")
						.arg(client->session()->id())));
		return QString();
	}

	if(cryptKey.isEmpty()) {
		return QStringLiteral("p%1").arg(
			QString::fromUtf8(compressed.toBase64()));
	} else {
#if HAVE_LIBSODIUM
		QByteArray nonce(compat::sizetype(crypto_secretbox_NONCEBYTES), 0);
		randombytes_buf(nonce.data(), nonce.size());

		QByteArray output(
			compat::sizetype(crypto_secretbox_MACBYTES) + compressed.size(), 0);
		int cryptResult = crypto_secretbox_easy(
			reinterpret_cast<unsigned char *>(output.data()),
			reinterpret_cast<const unsigned char *>(compressed.constData()),
			compressed.size(),
			reinterpret_cast<const unsigned char *>(nonce.constData()),
			reinterpret_cast<const unsigned char *>(cryptKey.constData()));
		if(cryptResult != 0) {
			logger->logMessage(
				Log()
					.user(
						client->id(), client->peerAddress(), client->username())
					.about(Log::Level::Error, Log::Topic::BanImpEx)
					.message(QStringLiteral(
								 "Error encrypting ban list from session %1")
								 .arg(client->session()->id())));
			return QString();
		}

		return QStringLiteral("c%1:%2").arg(
			QString::fromUtf8(nonce.toBase64()),
			QString::fromUtf8(output.toBase64()));
#else
		logger->logMessage(
			Log()
				.user(client->id(), client->peerAddress(), client->username())
				.about(Log::Level::Error, Log::Topic::BanImpEx)
				.message(
					QStringLiteral("Encrypted ban export requested for session "
								   "%1, but sodium is not compiled in!")
						.arg(client->session()->id())));
		return QString();
#endif
	}
}

CmdResult exportPlainBans(Client *client)
{
	if(!client->isModerator()) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral("Only moderators can export plain bans."),
			net::ServerReply::KEY_BANEXPORT_MODONLY));
	}

	const Session *session = client->session();
	const ServerConfig *config = session->config();
	QString result = buildBanExport(
		client, config->logger(), QByteArray(),
		QJsonDocument(session->getExportBanList())
			.toJson(QJsonDocument::Compact));
	if(result.isEmpty()) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral("Server error."),
			net::ServerReply::KEY_BANEXPORT_SERVERERROR));
	}

	config->logger()->logMessage(
		Log()
			.user(client->id(), client->peerAddress(), client->username())
			.about(Log::Level::Info, Log::Topic::BanImpEx)
			.message(QStringLiteral("Exported plain bans from session %1")
						 .arg(session->id())));
	client->sendDirectMessage(net::ServerReply::makeBanExportResult(result));
	return CmdResult::ok();
}

CmdResult exportEncryptedBans(Client *client)
{
#ifdef HAVE_LIBSODIUM
	const Session *session = client->session();
	const ServerConfig *config = session->config();
	QByteArray cryptKey = config->internalConfig().cryptKey;
	if(cryptKey.size() != compat::sizetype(crypto_secretbox_KEYBYTES)) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral(
				"Exporting encrypted bans not configured on this server."),
			net::ServerReply::KEY_BANEXPORT_UNCONFIGURED));
	}

	QString result = buildBanExport(
		client, config->logger(), cryptKey,
		QJsonDocument(session->getExportBanList())
			.toJson(QJsonDocument::Compact));
	if(result.isEmpty()) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral("Server error."),
			net::ServerReply::KEY_BANEXPORT_SERVERERROR));
	}

	config->logger()->logMessage(
		Log()
			.user(client->id(), client->peerAddress(), client->username())
			.about(Log::Level::Info, Log::Topic::BanImpEx)
			.message(QStringLiteral("Exported encrypted bans from session %1")
						 .arg(session->id())));
	client->sendDirectMessage(net::ServerReply::makeBanExportResult(result));
	return CmdResult::ok();
#else
	Q_UNUSED(client);
	return CmdResult::reply(net::ServerReply::makeBanImpExError(
		QStringLiteral(
			"Exporting encrypted bans not supported by this server."),
		net::ServerReply::KEY_BANEXPORT_UNSUPPORTED));
#endif
}

CmdResult
exportBans(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(args);
	bool plain = kwargs[QStringLiteral("plain")].toBool();
	return plain ? exportPlainBans(client) : exportEncryptedBans(client);
}

bool decodeBanImport(
	const QString &input, QByteArray &outNonce, QByteArray &outPayload)
{
	static QRegularExpression plainRe(
		QStringLiteral("\\Ap([a-zA-Z0-9+/=]+)\\z"));
	QRegularExpressionMatch plainMatch = plainRe.match(input);
	if(plainMatch.hasMatch()) {
		outNonce.clear();
		outPayload = QByteArray::fromBase64(plainMatch.captured(1).toUtf8());
		return !outPayload.isEmpty();
	}

	static QRegularExpression re(
		QStringLiteral("\\Ac([a-zA-Z0-9+/=]+):([a-zA-Z0-9+/=]+)\\z"));
	QRegularExpressionMatch match = re.match(input);
	if(!match.hasMatch()) {
		return false;
	}

	outNonce = QByteArray::fromBase64(match.captured(1).toUtf8());
#ifdef HAVE_LIBSODIUM
	bool badNonce =
		outNonce.size() != compat::sizetype(crypto_secretbox_NONCEBYTES);
#else
	bool badNonce = outNonce.isEmpty();
#endif
	if(badNonce) {
		return false;
	}

	outPayload = QByteArray::fromBase64(match.captured(2).toUtf8());
#ifdef HAVE_LIBSODIUM
	return outPayload.size() >= compat::sizetype(crypto_secretbox_MACBYTES);
#else
	return !outPayload.isEmpty();
#endif
}

CmdResult
importBans(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1) {
		return CmdResult::err("Expected one argument: bans");
	}

	Session *session = client->session();
	QByteArray nonce, payload;
	if(!decodeBanImport(args[0].toString(), nonce, payload)) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral("Malformed import data."),
			net::ServerReply::KEY_BANIMPORT_MALFORMED));
	}

	const ServerConfig *config = session->config();
	if(!nonce.isEmpty()) {
#ifdef HAVE_LIBSODIUM
		QByteArray cryptKey = config->internalConfig().cryptKey;
		if(cryptKey.size() != compat::sizetype(crypto_secretbox_KEYBYTES)) {
			return CmdResult::reply(net::ServerReply::makeBanImpExError(
				QStringLiteral(
					"Importing encrypted bans not configured on this server."),
				net::ServerReply::KEY_BANIMPORT_UNCONFIGURED));
		}

		int cryptResult = crypto_secretbox_open_easy(
			reinterpret_cast<unsigned char *>(payload.data()),
			reinterpret_cast<const unsigned char *>(payload.constData()),
			payload.size(),
			reinterpret_cast<const unsigned char *>(nonce.constData()),
			reinterpret_cast<const unsigned char *>(cryptKey.constData()));
		if(cryptResult != 0) {
			return CmdResult::reply(net::ServerReply::makeBanImpExError(
				QStringLiteral("The server couldn't read the import data. This "
							   "is likely because it was exported from a "
							   "different server. You can only import bans "
							   "into the same server they were exported from."),
				net::ServerReply::KEY_BANIMPORT_CRYPTERROR));
		}
		payload.resize(
			payload.size() - compat::sizetype(crypto_secretbox_MACBYTES));
#else
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral(
				"Importing encrypted bans not supported by this server."),
			net::ServerReply::KEY_BANIMPORT_UNSUPPORTED));
#endif
	}

	QJsonObject bans = QJsonDocument::fromJson(qUncompress(payload)).object();
	int total, imported;
	bool ok = session->importBans(bans, total, imported, client);
	if(!ok) {
		return CmdResult::reply(net::ServerReply::makeBanImpExError(
			QStringLiteral("Invalid import data."),
			net::ServerReply::KEY_BANIMPORT_INVALID));
	}

	config->logger()->logMessage(
		Log()
			.user(client->id(), client->peerAddress(), client->username())
			.about(Log::Level::Info, Log::Topic::BanImpEx)
			.message(QStringLiteral("Imported %1/%2 bans into session %3")
						 .arg(
							 QString::number(imported), QString::number(total),
							 session->id())));
	client->sendDirectMessage(
		net::ServerReply::makeBanImportResult(total, imported));

	return CmdResult::ok();
}

CmdResult
authList(Client *client, const QJsonArray &args, const QJsonObject &kwargs)
{
	Q_UNUSED(kwargs);
	if(args.size() != 1) {
		return CmdResult::err("Expected one argument: list");
	}

	QJsonArray list = args.at(0).toArray();
	if(list.size() > 100) {
		return CmdResult::err("Insane argument");
	}

	Session *session = client->session();
	SessionHistory *history = session->history();
	bool sendUpdate = false;

	qDebug() << list;
	for(const QJsonValue &value : list) {
		QString authId = value[QStringLiteral("a")].toString();
		qDebug() << authId;
		if(!authId.isEmpty()) {
			Client *target = session->getClientByAuthId(authId);

			if(client != target) {
				QJsonValue opValue = value[QStringLiteral("o")];
				if(opValue.isBool()) {
					bool op = opValue.toBool();
					if(target) {
						if(target->isOperator() != op) {
							sendUpdate = true;
							session->changeOpStatus(
								target->id(), op, client->username(), false);
						}
					} else {
						sendUpdate = true;
						history->setAuthenticatedOperator(authId, op);
					}
				}
			}

			QJsonValue trustedValue = value[QStringLiteral("t")];
			if(trustedValue.isBool()) {
				bool trusted = trustedValue.toBool();
				if(target) {
					if(target->isTrusted() != trusted) {
						sendUpdate = true;
						session->changeTrustedStatus(
							target->id(), trusted, client->username(), false);
					}
				} else {
					sendUpdate = true;
					history->setAuthenticatedTrust(authId, trusted);
				}
			}

			if(!target) {
				QJsonValue usernameValue = value[QStringLiteral("u")];
				if(usernameValue.isString()) {
					QString username = usernameValue.toString();
					if(!username.isEmpty()) {
						sendUpdate = true;
						history->setAuthenticatedUsername(authId, username);
					}
				}
			}
		}
	}

	if(sendUpdate) {
		client->session()->sendUpdatedAuthList();
	}

	return CmdResult::ok();
}

SrvCommandSet::SrvCommandSet()
{
	commands << SrvCommand("ready-to-autoreset", readyToAutoReset)
			 << SrvCommand("init-begin", initBegin)
			 << SrvCommand("init-complete", initComplete)
			 << SrvCommand("init-cancel", initCancel)
			 << SrvCommand("sessionconf", sessionConf)
			 << SrvCommand("kick-user", kickUser, SrvCommand::DEPUTY)
			 << SrvCommand("gain-op", opWord, SrvCommand::NONOP)
			 << SrvCommand("reset-session", resetSession)
			 << SrvCommand("kill-session", killSession, SrvCommand::MOD)
			 << SrvCommand("announce-session", announceSession)
			 << SrvCommand("unlist-session", unlistSession)
			 << SrvCommand("remove-ban", removeBan)
			 << SrvCommand("mute", setMute)
			 << SrvCommand("report", reportAbuse, SrvCommand::NONOP)
			 << SrvCommand("export-bans", exportBans)
			 << SrvCommand("import-bans", importBans)
			 << SrvCommand("auth-list", authList);
}

} // end of anonymous namespace

void handleClientServerCommand(
	Client *client, const QString &command, const QJsonArray &args,
	const QJsonObject &kwargs)
{
	for(const SrvCommand &c : COMMANDS.commands) {
		if(c.name() == command) {
			if(c.mode() == SrvCommand::MOD && !client->isModerator()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command, QStringLiteral("Not a moderator")));
				return;
			} else if(c.mode() == SrvCommand::OP && !client->isOperator()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command, QStringLiteral("Not a session owner")));
				return;
			} else if(
				c.mode() == SrvCommand::DEPUTY && !client->isOperator() &&
				!client->isDeputy()) {
				client->sendDirectMessage(net::ServerReply::makeCommandError(
					command,
					QStringLiteral("Not a session owner or a deputy")));
				return;
			}

			CmdResult result = c.call(client, args, kwargs);
			if(!result.success()) {
				client->sendDirectMessage(result.getReply(command));
			}
			return;
		}
	}

	client->sendDirectMessage(
		net::ServerReply::makeCommandError(command, "Unknown command"));
}
}
